#ifndef _MOL_DEF_GUARD_DEFINE_REPRORABBIT_DEF_
#define _MOL_DEF_GUARD_DEFINE_REPRORABBIT_DEF_

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
#include "priocpp/api.h"
#include "priocpp/ResourcePool.h"
//#include "priocpp/impl/event.h"
//#include "priocpp/task.h"
//#include <signal.h>

//#include <event2/event.h>
//#include <amqpcpp.h>
//#include <amqpcpp/libevent.h>

using namespace repro;
using namespace prio;

namespace AMQP 
{
    class Message;
    class TcpConnection;
    class TcpChannel;
    class LibEventHandler;
}

namespace reprorabbit 
{

class RabbitChannel;

/*
class RabbitChannel
{
public:

  std::shared_ptr<AMQP::TcpConnection> con;
  std::shared_ptr<AMQP::TcpChannel> channel;

};
*/

struct RabbitLocator
{
	typedef RabbitChannel type;

	static repro::Future<type*> retrieve(const std::string& url);
	static void free( type* t);

    static AMQP::LibEventHandler* handler();
};

/*
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
*/

class RabbitPool;

class RabbitTransaction
{
public:

    RabbitTransaction(std::shared_ptr<prio::Resource<RabbitChannel>> rabbit);

    RabbitTransaction& publish( std::string exchange, std::string key, std::string msg);

    Future<> commit();

private:

   std::shared_ptr<prio::Resource<RabbitChannel>> rabbit_;
};

class RabbitPool
{
public:
	typedef prio::ResourcePool<RabbitLocator> Pool;
	typedef Pool::ResourcePtr ResourcePtr;

	RabbitPool(const std::string& url, int capacity = 4);
	RabbitPool();
	~RabbitPool();

    repro::Future<RabbitPool::ResourcePtr> get();

    repro::Future<> publish( std::string exchange, std::string key, std::string msg);

    repro::Future<RabbitTransaction> tx();

	void shutdown();

private:

	std::string url_;
	Pool pool_;
};

class RabbitMsg
{
public:

  RabbitPool::ResourcePtr rabbit;

  const AMQP::Message &message;
  uint64_t deliveryTag;
  bool redelivered;

  void ack();
  void reject();

  std::string body();
};


class RabbitListener
{
public:

  RabbitListener(std::shared_ptr<RabbitPool> pool);

  Future<RabbitMsg> subscribe(std::string queue);

private:

  Promise<RabbitMsg> p_;
  std::shared_ptr<RabbitPool> pool_;
};

}

#endif
