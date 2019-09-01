#include "gtest/gtest.h"
#include "test.h"
#include "reprorabbit/rabbit.h"
#include <iostream>

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
      Exchange exchange("test-exchange");

      exchange
      .bind("","test-key")
      .create(*pool)
      .then([&pool]()
      {
        Queue queue("test");
        
        return queue
        .bind("test-exchange","test-key")
        .create(*pool);
      })
      .then([&pool,&listener]()
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

#ifdef _RESUMABLE_FUNCTIONS_SUPPORTED

repro::Future<> coro_test(std::shared_ptr<RabbitPool> pool, RabbitListener& listener)
{
    Queue queue("test");

    co_await queue.create(*pool);

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

