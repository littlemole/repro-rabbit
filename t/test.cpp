#include "gtest/gtest.h"
#include "test.h"
#include "reprorabbit/rabbit.h"
#include <iostream>

#include <amqpcpp/flags.h>

using namespace reprorabbit;
using namespace repro;
using namespace prio;


class BasicTest : public ::testing::Test {
 protected:


  virtual void SetUp() {
  }

  virtual void TearDown() {
//	    MOL_TEST_PRINT_CNTS();
	//	MOL_TEST_ASSERT_CNTS(0,0);
  }
}; // end test setup


TEST_F(BasicTest, SimpleRabbit) 
{

    const char* RABBIT_HOST = "localhost";

    const char* env_host = getenv("RABBIT_HOST");
    if(env_host)
    {
        RABBIT_HOST = env_host;
    }

  {

    prio::signal(SIGINT).then([](int){});

   // RabbitLocator::handler();

    std::ostringstream oss;
    oss << "amqp://" << RABBIT_HOST;

    std::string url = oss.str();

    std::cout << "url: " << url << std::endl;


    auto pool = std::make_shared<RabbitPool>(url);

    RabbitListener listener(pool);

    nextTick([&pool,&listener]()
    {
      Exchange("test-exchange")
      .bind("","test-key")
      .create(*pool)
      .then([&pool]()
      {        
        return Queue("test")
        .flags(AMQP::autodelete)
        .bind("test-exchange","test-key")
        .create(*pool);
      })
      .then([&pool,&listener](QueueStatus qs)
      {
        listener.subscribe("test")
        .then([](RabbitMsg msg )
        {
              std::cout << "message delivered" << std::endl;
              std::cout << msg.body() << std::endl;
              msg.ack();

              static int i = 0;
              i++;

              if(i>2)
              {
                timeout([]()
                {
                    theLoop().exit();
                },0,50);
              }
        });

        pool->publish("test-exchange","test-key","msg number one")
        .then([]()
        {
            std::cout << "Published" << std::endl;
        });

        pool->publish("test-exchange","test-key","msg number two")
        .then([]()
        {
            std::cout << "Published" << std::endl;
        })
        .otherwise([](const repro::Ex& ex)
        {
          std::cout << "failed" << std::endl;
        });


        pool->tx()
        .then([](RabbitTransaction tx)
        {
          tx.publish("test-exchange","test-key","msg number three");
          return tx.commit();
        })      
        .then([]()
        {
            std::cout << "Published" << std::endl;
        });
      });
    });

    theLoop().run();
  }

  MOL_TEST_PRINT_CNTS();

}



TEST_F(BasicTest, SimpleRabbitFan) 
{
    static int i = 0;


    const char* RABBIT_HOST = "localhost";

    const char* env_host = getenv("RABBIT_HOST");
    if(env_host)
    {
        RABBIT_HOST = env_host;
    }

  {

    prio::signal(SIGINT).then([](int){});

   // RabbitLocator::handler();

    std::ostringstream oss;
    oss << "amqp://" << RABBIT_HOST;

    std::string url = oss.str();

    std::cout << "url: " << url << std::endl;


    auto pool = std::make_shared<RabbitPool>(url);

    RabbitListener listener1(pool);
    RabbitListener listener2(pool);

    nextTick([&pool,&listener1,&listener2]()
    {
      Exchange("fan-exchange")
      .type(AMQP::fanout)
      .bind("","fan-key")
      .create(*pool)
      .then([&pool]()
      {
        return Queue()
        .flags(AMQP::autodelete)
        .bind("fan-exchange","fan-key")
        .create(*pool);
      })
      .then([&pool,&listener1](QueueStatus qs)
      {
        listener1.subscribe(qs.name)
        .then([](RabbitMsg msg )
        {
              std::cout << "message delivered" << std::endl;
              std::cout << msg.body() << std::endl;
              msg.ack();

              i++;

              if(i>1)
              {
                timeout([]()
                {
                    theLoop().exit();
                },0,50);
              }
        });

        return Queue()
        .flags(AMQP::autodelete)
        .bind("fan-exchange","fan-key")
        .create(*pool);
      })
      .then([&pool,&listener2](QueueStatus qs)
      {
        listener2.subscribe(qs.name)
        .then([](RabbitMsg msg )
        {
              std::cout << "message delivered" << std::endl;
              std::cout << msg.body() << std::endl;
              msg.ack();

              i++;

              if(i>1)
              {
                timeout([]()
                {
                    theLoop().exit();
                },0,50);
              }
        });
        pool->publish("fan-exchange","fan-key","msg number one")
        .then([]()
        {
            std::cout << "Published" << std::endl;
        });

      });
    });

    theLoop().run();
  }

  MOL_TEST_PRINT_CNTS();

}

#ifdef _RESUMABLE_FUNCTIONS_SUPPORTED

repro::Future<> coro_test(std::shared_ptr<RabbitPool> pool, RabbitListener& listener)
{
    QueueStatus qs = co_await Queue("test").flags(AMQP::autodelete).create(*pool);

    listener.subscribe("test")
    .then([](RabbitMsg msg )
    {
          std::cout << "message delivered" << std::endl;
          std::cout << msg.body() << std::endl;
          msg.ack();

          static int i = 0;
          i++;

          if(i>2)
          {
            timeout([]()
            {
                theLoop().exit();
            },0,50);
          }
    });

    co_await pool->publish("","test","msg number one");

    std::cout << "Published" << std::endl;
    
    co_await pool->publish("","test","msg number two");
    
    std::cout << "Published" << std::endl;

    RabbitTransaction tx = co_await pool->tx();

    tx.publish("","test","msg number three");

    co_await tx.commit();

    co_return;
}

TEST_F(BasicTest, CoroTest) 
{

    const char* RABBIT_HOST = "localhost";

    const char* env_host = getenv("RABBIT_HOST");
    if(env_host)
    {
        RABBIT_HOST = env_host;
    }

  {

    prio::signal(SIGINT).then([](int){});

   // RabbitLocator::handler();

    std::ostringstream oss;
    oss << "amqp://" << RABBIT_HOST;

    std::string url = oss.str();

    std::cout << "url: " << url << std::endl;


    auto pool = std::make_shared<RabbitPool>(url);

    RabbitListener listener(pool);

    nextTick([&pool,&listener](){

        coro_test(pool,listener)
        .then([](){});
    });


    theLoop().run();
  }

  MOL_TEST_PRINT_CNTS();

}

#endif



int main(int argc, char **argv) 
{
    prio::Libraries<prio::EventLoop> init;

    ::testing::InitGoogleTest(&argc, argv);
    int r = RUN_ALL_TESTS();

    return r;
}

