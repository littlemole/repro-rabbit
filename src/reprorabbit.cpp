#include "reprorabbit/rabbit.h"

//#include <memory>
//#include <list>
//#include <utility>
//#include <iostream>
//#include <string>
//#include <exception>
//#include <functional>
//#include "reprocpp/after.h"
//#include "test.h"
//#include "promise/asio/loop.h"
//#include "priocpp/api.h"
//#include "priocpp/ResourcePool.h"
#include "priocpp/impl/event.h"
#include "priocpp/task.h"
//#include <signal.h>

//#include <event2/event.h>
#include <amqpcpp.h>
#include <amqpcpp/libevent.h>

namespace reprorabbit 
{

class RabbitChannel
{
public:

  std::shared_ptr<AMQP::TcpConnection> con;
  std::shared_ptr<AMQP::TcpChannel> channel;

};

AMQP::LibEventHandler* RabbitLocator::handler()
{
  static AMQP::LibEventHandler theHandler(eventLoop().base());
  return &theHandler;
}

repro::Future<RabbitChannel*> RabbitLocator::retrieve(const std::string& u)
{
  static std::mutex handler_mutex;;

  auto p = promise<RabbitChannel*>();

  std::string url = u;
  
  AMQP::LibEventHandler* h = handler();

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

        auto startCb = [](const std::string &consumertag) {

            std::cout << "consume operation started" << std::endl;
        };

        auto errorCb = [this,rabbit,queue](const char *message) {

            rabbit->markAsInvalid();

            (*rabbit)->channel->close();
            (*rabbit)->channel.reset();

            timeout([this,queue,rabbit](){
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



}

