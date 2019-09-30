#include "reprorabbit/rabbit.h"

#include "priocpp/task.h"
#include <amqpcpp.h>

#ifdef PROMISE_USE_LIBEVENT  
#include "priocpp/impl/event.h"
#include <amqpcpp/libevent.h>
#endif
#ifdef PROMISE_USE_BOOST_ASIO
#include "priocpp/impl/asio.h"
#include <amqpcpp/libboostasio.h>
#endif

namespace reprorabbit 
{

////////////////////////////////////////////////////////////////////

class RabbitChannel
{
public:

    std::shared_ptr<AMQP::TcpConnection> con;
    std::shared_ptr<AMQP::TcpChannel> channel;

};

////////////////////////////////////////////////////////////////////

#ifdef PROMISE_USE_LIBEVENT    
AMQP::LibEventHandler* RabbitLocator::handler()
{
    static AMQP::LibEventHandler theHandler(eventLoop().base());
    return &theHandler;
}
#endif
#ifdef PROMISE_USE_BOOST_ASIO
AMQP::LibBoostAsioHandler* RabbitLocator::handler()
{
    static AMQP::LibBoostAsioHandler theHandler(asioLoop().io());
    return &theHandler;
}
#endif


repro::Future<RabbitChannel*> RabbitLocator::retrieve(const std::string& u)
{
  static std::mutex handler_mutex;;

  auto p = promise<RabbitChannel*>();

  std::string url = u;
  
  auto h = handler();

  task([url,h]() 
  {
      std::lock_guard<std::mutex> guard(handler_mutex);
      auto c = new AMQP::TcpConnection( h, AMQP::Address(url.c_str()));
      return c;
  })
  .then([p](AMQP::TcpConnection* c)
  {
      RabbitChannel* rc = new RabbitChannel();
      rc->con = std::shared_ptr<AMQP::TcpConnection>( c);
      rc->channel = std::make_shared<AMQP::TcpChannel>(rc->con.get());
      p.resolve(rc);
  });

  return p.future();;
}

void RabbitLocator::free(RabbitChannel* rc)
{
    if(rc->channel)
    {
        rc->channel->close();
    }
    if(rc->con)
    {
        rc->con->close();
    }
    delete rc;
}


////////////////////////////////////////////////////////////////////


RabbitPool::RabbitPool(const std::string& url, int capacity)
    : url_(url), pool_(capacity)
{}

RabbitPool::RabbitPool() {}

RabbitPool::~RabbitPool() {}


repro::Future<> RabbitPool::publish( std::string exchange, std::string key, std::string msg)
{
    auto p = promise();

    get()
    .then([p,exchange,key,msg](RabbitPool::ResourcePtr rabbit)
    {
        (*rabbit)->channel->startTransaction();
        (*rabbit)->channel->publish(exchange,key,msg);

        (*rabbit)->channel->commitTransaction()
        .onSuccess([p]() {
            p.resolve();
        })
        .onError([rabbit,p](const char *message) {
            rabbit->markAsInvalid();
            p.reject(repro::Ex(message));
        });      
    });      

    return p.future();
}

repro::Future<RabbitTransaction> RabbitPool::tx()
{
    auto p = promise<RabbitTransaction>();

    get()
    .then([p](RabbitPool::ResourcePtr rabbit)
    {
        p.resolve(RabbitTransaction(rabbit));
    });      

    return p.future();
}

void RabbitPool::shutdown()
{
    pool_.shutdown();
}



repro::Future<RabbitPool::ResourcePtr> RabbitPool::get()
{
	auto p = repro::promise<ResourcePtr>();
	pool_.get(url_)
	.then( [p](ResourcePtr r)
	{
		p.resolve(r);
	})
	.otherwise( [p](const std::exception& ex)
	{
		p.reject(ex);
	});

	return p.future();
}

////////////////////////////////////////////////////////////////////


RabbitTransaction::RabbitTransaction(RabbitPool::ResourcePtr rabbit)
: rabbit_(rabbit)
{
    (*rabbit_)->channel->startTransaction();
}

RabbitTransaction& RabbitTransaction::publish( std::string exchange, std::string key, std::string msg)
{
    (*rabbit_)->channel->publish(exchange,key,msg);
    return *this;
}

Future<> RabbitTransaction::commit()
{
    auto p = promise();

    RabbitPool::ResourcePtr r = rabbit_;

    (*r)->channel->commitTransaction()
    .onSuccess( [p,r]() 
    {
        p.resolve();
    })
    .onError([p,r](const char *message) 
    {
        r->markAsInvalid();
        p.reject(repro::Ex(message));
    });      

    return p.future();      
}

Future<> RabbitTransaction::rollback()
{
    auto p = promise();

    RabbitPool::ResourcePtr r = rabbit_;

    (*r)->channel->rollbackTransaction()
    .onSuccess( [p,r]() 
    {
        p.resolve();
    })
    .onError([p,r](const char *message) 
    {
        r->markAsInvalid();
        p.reject(repro::Ex(message));
    });      

    return p.future();      
}


////////////////////////////////////////////////////////////////////

void RabbitMsg::ack()
{
    AMQP::TcpChannel& channel = *((*rabbit)->channel);
    channel.ack(deliveryTag);
}

void RabbitMsg::reject()
{
    AMQP::TcpChannel& channel = *((*rabbit)->channel);
    channel.reject(deliveryTag);
}  

std::string RabbitMsg::body()
{
    return std::string(message.body(),message.bodySize());
}

const std::string& RabbitMsg::exchange() const
{
    return message.exchange();
}

const std::string& RabbitMsg::routingkey() const
{
    return message.routingkey();
} 

////////////////////////////////////////////////////////////////////


RabbitListener::RabbitListener(std::shared_ptr<RabbitPool> pool)
    : pool_(pool)
{
    p_ = promise<RabbitMsg>();
}

Future<RabbitMsg> RabbitListener::subscribe(std::string queue)
{
    pool_->get()
    .then([this,queue](RabbitPool::ResourcePtr rabbit)
    {
        AMQP::TcpChannel& channel = *((*rabbit)->channel);

        auto messageCb = [this,rabbit](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) 
        {
            p_.resolve(RabbitMsg{rabbit,message,deliveryTag,redelivered});
        };

        auto startCb = [](const std::string &consumertag) 
        {
            std::cout << "consume operation started" << std::endl;
        };

        auto errorCb = [this,rabbit,queue](const char *message) 
        {
            rabbit->markAsInvalid();

            (*rabbit)->channel->close();
            (*rabbit)->channel.reset();

            timeout([this,queue,rabbit]()
            {
                subscribe(queue);
            },5,0);
        };


        channel.consume(queue)
        .onReceived(messageCb)
        .onSuccess(startCb);

        channel.onError(errorCb);
    });

    return p_.future();
}


//static AMQP::Table defaultTable;
Exchange::~Exchange()
{}

Exchange::Exchange(const std::string& name)
    : name_(name), type_(new AMQP::ExchangeType(AMQP::direct)), flags_(0), arguments_(new AMQP::Table()), bind_arguments_(new AMQP::Table())
{}
    
Exchange& Exchange::type(const AMQP::ExchangeType& t)
{
    type_.reset(new AMQP::ExchangeType(t));
    return *this;
}
            
Exchange& Exchange::flags(int f)
{
    flags_ = f;
    return *this;
}
            
Exchange& Exchange::arguments(const AMQP::Table& arguments)
{
    arguments_.reset(new AMQP::Table(arguments));
    return *this;
}

Exchange& Exchange::bind( const std::string& exchange, const std::string& routing_key, const AMQP::Table& arguments)
{
    exchange_ = exchange;
    routing_key_ = routing_key;
    bind_arguments_.reset(new AMQP::Table(arguments));
    return *this;
}

Exchange& Exchange::bind( const std::string& exchange, const std::string& routing_key)
{
    AMQP::Table defaultTable;
    return bind(exchange,routing_key,defaultTable);
}

repro::Future<> Exchange::create (RabbitPool& rabbit)
{
    auto p = repro::promise();

    std::string name = name_;
    AMQP::ExchangeType type = *type_;
    int flags = flags_;
    AMQP::Table arguments = *arguments_;

    std::string exchange = exchange_;
    std::string routing_key = routing_key_;
    AMQP::Table bind_arguments = *bind_arguments_;

    rabbit.get()
    .then( [p,name,type,flags,arguments,exchange,routing_key,bind_arguments](RabbitPool::ResourcePtr channel)
    {    
        (*channel)->channel->declareExchange(name,type,flags,arguments)
        .onSuccess([p,channel,name,exchange,routing_key,bind_arguments]() 
        {
            if(!exchange.empty())
            {
                (*channel)->channel->bindExchange(exchange,name,routing_key,bind_arguments)
                .onSuccess([p]() 
                {
                    p.resolve();
                })
                .onError([channel,p](const char *message) 
                {
                    channel->markAsInvalid();
                    p.reject(repro::Ex(message));
                }); 
            }
            else
            {
                p.resolve();
            }
        })
        .onError([channel,p](const char *message) 
        {
            channel->markAsInvalid();
            p.reject(repro::Ex(message));
        });   
    })
    .otherwise(reject(p));

    return p.future();
}

Queue::~Queue()
{

}

Queue::Queue()
    : name_(""), flags_(0), arguments_(new AMQP::Table()),bind_arguments_(new AMQP::Table())
{}


Queue::Queue(const std::string& name)
    : name_(name), flags_(0), arguments_(new AMQP::Table()),bind_arguments_(new AMQP::Table())
{}

Queue& Queue::flags(int f)
{
    flags_ = f;
    return *this;
}

Queue& Queue::arguments( const AMQP::Table& arguments)
{
    arguments_.reset(new AMQP::Table(arguments));
    return *this;
}

Queue& Queue::bind( const std::string& exchange, const std::string& routing_key, const AMQP::Table& arguments)
{
    exchange_ = exchange;
    routing_key_ = routing_key;
    bind_arguments_.reset(new AMQP::Table(arguments));
    return *this;
}

Queue& Queue::bind( const std::string& exchange, const std::string& routing_key)
{
    AMQP::Table defaultTable;
    return bind(exchange,routing_key,defaultTable);
}

repro::Future<QueueStatus> Queue::create (RabbitPool& rabbit)
{
    auto p = repro::promise<QueueStatus>();

    std::string name = name_;
    int flags = flags_;
    AMQP::Table arguments = *arguments_;

    std::string exchange = exchange_;
    std::string routing_key = routing_key_;
    AMQP::Table bind_arguments = *bind_arguments_;

    rabbit.get()
    .then( [p,name,flags,arguments,exchange, routing_key, bind_arguments](RabbitPool::ResourcePtr channel)
    {
        (*channel)->channel->declareQueue(name,flags,arguments)
        .onSuccess([p,channel,name,exchange, routing_key, bind_arguments](const std::string &queuename, uint32_t messagecount, uint32_t consumercount) 
        {
            QueueStatus qs {queuename,messagecount,consumercount};
            if(!exchange.empty())
            {
                (*channel)->channel->bindQueue(exchange,name,routing_key,bind_arguments)
                .onSuccess([p,qs]() 
                {
                    p.resolve(qs);
                })
                .onError([channel,p](const char *message) 
                {
                    channel->markAsInvalid();
                    p.reject(repro::Ex(message));
                }); 
            }
            else
            {
                p.resolve(qs);
            }
        })
        .onError([channel,p](const char *message) 
        {
            channel->markAsInvalid();
            p.reject(repro::Ex(message));
        });   
    })
    .otherwise(reject(p));

    return p.future();
}


}

