#ifndef _MOL_DEF_GUARD_DEFINE_REPRORABBIT_DEF_
#define _MOL_DEF_GUARD_DEFINE_REPRORABBIT_DEF_

#include "priocpp/api.h"
#include "priocpp/ResourcePool.h"


using namespace repro;
using namespace prio;

////////////////////////////////////////////////////////////////////

namespace AMQP 
{
    class Message;
    class TcpConnection;
    class TcpChannel;
#ifdef PROMISE_USE_LIBEVENT    
    class LibEventHandler;
#endif
#ifdef PROMISE_USE_BOOST_ASIO
    class LibBoostAsioHandler;
#endif
}

////////////////////////////////////////////////////////////////////

namespace reprorabbit 
{

////////////////////////////////////////////////////////////////////

class RabbitChannel;
class RabbitPool;

////////////////////////////////////////////////////////////////////

struct RabbitLocator
{
	typedef RabbitChannel type;

	static repro::Future<type*> retrieve(const std::string& url);
	static void free( type* t);

#ifdef PROMISE_USE_LIBEVENT  
    static AMQP::LibEventHandler* handler();
#endif
#ifdef PROMISE_USE_BOOST_ASIO
    static AMQP::LibBoostAsioHandler* handler();
#endif
};


////////////////////////////////////////////////////////////////////

class RabbitTransaction
{
public:

    RabbitTransaction(std::shared_ptr<prio::Resource<RabbitChannel>> rabbit);

    RabbitTransaction& publish( std::string exchange, std::string key, std::string msg);

    Future<> commit();

private:

   std::shared_ptr<prio::Resource<RabbitChannel>> rabbit_;
};


////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////

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


////////////////////////////////////////////////////////////////////

class RabbitListener
{
public:

  RabbitListener(std::shared_ptr<RabbitPool> pool);

  Future<RabbitMsg> subscribe(std::string queue);

private:

  Promise<RabbitMsg> p_;
  std::shared_ptr<RabbitPool> pool_;
};


} // end namespace

#endif
