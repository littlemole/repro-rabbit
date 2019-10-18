#include "gtest/gtest.h"
#include "reprocpp/test.h"
#include "reprorabbit/rabbit.h"
#include <iostream>

#include <amqpcpp/flags.h>
#include <amqpcpp/table.h>

using namespace reprorabbit;
using namespace repro;
using namespace prio;


class BasicTest : public ::testing::Test 
{
protected:

    std::shared_ptr<RabbitPool> pool;

    virtual void SetUp() 
    {
        prio::signal(SIGINT).then([](int)
        {
          std::cout << "SIGINT" << std::endl;
          theLoop().exit();
        });      

        pool = std::make_shared<RabbitPool>(getRabbitUrl());
    }

    virtual void TearDown() 
    {
        pool.reset();
        MOL_TEST_ASSERT_CNTS(0,0);
    }

    void count_message(RabbitMsg& msg, int& i, int max)
    {
        std::cout << "message delivered" << std::endl;
        std::cout << msg.body() << std::endl;
        msg.ack();

        i++;

        if(i>max)
        {
          timeout([]()
          {
              theLoop().exit();
          },0,500);
        }
    }

    void publish( 
        std::string exchange,
        std::string routing_key,
        std::string msg )
    {
        pool->publish( exchange, routing_key, msg )
        .then([]()
        {
            std::cout << "Published" << std::endl;
        })
        .otherwise([](const repro::Ex& ex)
        {
          std::cout << "failed" << std::endl;
        });    
    }

private:

    std::string getRabbitUrl()
    {
      const char* RABBIT_HOST = "localhost";

      const char* env_host = getenv("RABBIT_HOST");
      if(env_host)
      {
          RABBIT_HOST = env_host;
      }

      std::ostringstream oss;
      oss << "amqp://" << RABBIT_HOST;

      return oss.str();
    }

}; // end test setup


TEST_F(BasicTest, SimpleRabbit) 
{
    {
        RabbitListener listener(pool);

        nextTick()
        .then([this]()
        {
            return Exchange("test-exchange")
            .bind("","test-key")
            .create(*pool);
        })
        .then([this]()
        {        
            return Queue("test")
            .flags(AMQP::autodelete)
            .bind("test-exchange","test-key")
            .create(*pool);
        })
        .then([this,&listener](QueueStatus qs)
        {
            listener.subscribe("test")
            .then([this](RabbitMsg msg )
            {
                static int i = 0;
                count_message(msg,i,2);
            });

            publish("test-exchange","test-key","msg number one");
            publish("test-exchange","test-key","msg number two");

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

        theLoop().run();
    }

    MOL_TEST_PRINT_CNTS();
}



TEST_F(BasicTest, SimpleRabbitFan) 
{
    static int i = 0;

    {
        RabbitListener listener1(pool);
        RabbitListener listener2(pool);

        nextTick()
        .then([this]()
        {
            return Exchange("fan-exchange")
            .type(AMQP::fanout)
            .bind("","fan-key")
            .create(*pool);
        })
        .then([this]()
        {
            return Queue()
            .flags(AMQP::autodelete)
            .bind("fan-exchange","fan-key")
            .create(*pool);
        })
        .then([this,&listener1](QueueStatus qs)
        {
            listener1.subscribe(qs.name)
            .then([this](RabbitMsg msg )
            {
                count_message(msg,i,1);
            });

            return Queue()
            .flags(AMQP::autodelete)
            .bind("fan-exchange","fan-key")
            .create(*pool);
        })
        .then([this,&listener2](QueueStatus qs)
        {
            listener2.subscribe(qs.name)
            .then([this](RabbitMsg msg )
            {
                EXPECT_EQ("msg number one",msg.body());
                count_message(msg,i,1);
            });

            publish("fan-exchange","fan-key","msg number one");
        });

        theLoop().run();
    }
    MOL_TEST_PRINT_CNTS();
}


TEST_F(BasicTest, SimpleRabbitTopic) 
{
    static int i = 0;

    {
      RabbitListener listener1(pool);
      RabbitListener listener2(pool);

      nextTick()
      .then([this]()
      {
          return Exchange("topic-exchange")
          .type(AMQP::topic)
          .create(*pool);
      })
      .then([this]()
      {
          return Queue()
          .flags(AMQP::autodelete)
          .bind("topic-exchange","topic.one")
          .create(*pool);
      })
      .then([this,&listener1](QueueStatus qs)
      {
          listener1.subscribe(qs.name)
          .then([this](RabbitMsg msg )
          {
              EXPECT_EQ("msg number one",msg.body());
              count_message(msg,i,1);
          });

          return Queue()
          .flags(AMQP::autodelete)
          .bind("topic-exchange","topic.two")
          .create(*pool);
      })
      .then([this,&listener2](QueueStatus qs)
      {
          listener2.subscribe(qs.name)
          .then([this](RabbitMsg msg )
          {
              EXPECT_EQ("msg number two",msg.body());
              count_message(msg,i,1);
          });

          publish("topic-exchange","topic.one","msg number one");
          publish("topic-exchange","topic.two","msg number two");
      });

      theLoop().run();
    }

    MOL_TEST_PRINT_CNTS();

}


TEST_F(BasicTest, SimpleRabbitDeadLettering) 
{
    static int i = 0;

    {
        RabbitListener listener1(pool);
        RabbitListener listener2(pool);

        nextTick()
        .then([this]()
        {
            return Exchange("dlx-exchange")
            .type(AMQP::topic)
            .create(*pool);
        })
        .then([this]()
        {
            return Queue()
            .flags(AMQP::autodelete)
            .bind("dlx-exchange","#")
            .create(*pool);
        })
        .then([this,&listener1](QueueStatus qs)
        {
            listener1.subscribe(qs.name)
            .then([this](RabbitMsg msg )
            {
                std::cout << "msg arrived in DL" << std::endl;
                EXPECT_EQ("msg number one",msg.body());
                count_message(msg,i,0);
            });
/*        });
        
        nextTick()
        .then([this]()
        {
            */
            AMQP::Table arguments;
            arguments["x-dead-letter-exchange"] = "dlx-exchange";

            return Queue("test-dlx")
            .flags(AMQP::autodelete)
            .arguments(arguments)
            .create(*pool);
        })
        .then([this,&listener2](QueueStatus qs)
        {
            listener2.subscribe(qs.name)
            .then([this](RabbitMsg msg )
            {
                std::cout << "msg arrived in Queue" << std::endl;
                EXPECT_EQ("msg number one",msg.body());
                msg.reject();
                //count_message(msg,i,1);
            });

            publish("","test-dlx","msg number one");
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
  {
    RabbitListener listener(pool);

    nextTick([this,&listener]()
    {
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

