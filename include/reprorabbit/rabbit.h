#ifndef _MOL_DEF_GUARD_DEFINE_REPRORABBIT_DEF_
#define _MOL_DEF_GUARD_DEFINE_REPRORABBIT_DEF_

#include "priocpp/api.h"
#include "priocpp/ResourcePool.h"
#include <amqpcpp/exchangetype.h>

using namespace repro;
using namespace prio;

////////////////////////////////////////////////////////////////////

namespace AMQP 
{
    enum ExchangeType;
    class Table;
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

    Future<> rollback();

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


class Exchange
{
public:

    Exchange(const std::string& name);
        
    Exchange& type( AMQP::ExchangeType& type);
    Exchange& flags(int type);                
    Exchange& arguments( AMQP::Table& arguments);

    Exchange& bind( const std::string& exchange, const std::string& routing_key, AMQP::Table& arguments);
    Exchange& bind( const std::string& exchange, const std::string& routing_key);

    repro::Future<> create (RabbitPool& rabbit);


private:
    std::string name_;
    AMQP::ExchangeType type_;
    int flags_;
    AMQP::Table& arguments_;

    std::string exchange_;
    std::string routing_key_;
    AMQP::Table& bind_arguments_;

    RabbitPool::ResourcePtr channel_;
};

class Queue
{
public:
    Queue(const std::string& name);

    Queue& flags(int f);
    Queue& arguments( AMQP::Table& arguments);

    Queue& bind( const std::string& exchange, const std::string& routing_key, AMQP::Table& arguments);
    Queue& bind( const std::string& exchange, const std::string& routing_key);

    repro::Future<> create (RabbitPool& rabbit);

private:
    std::string name_;
    int flags_;
    AMQP::Table& arguments_;

    std::string exchange_;
    std::string routing_key_;
    AMQP::Table& bind_arguments_;     
};



} // end namespace

#endif
