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


TEST_F(BasicTest, SimpleHttp) 
{

  {

    prio::signal(SIGINT).then([](int){});

    RabbitLocator::handler();

    auto pool = std::make_shared<RabbitPool>("amqp://localhost/");

    nextTick([&pool](){

      pool->publish("","test","msg number one")
      .then([&pool]()
      {
          std::cout << "Published" << std::endl;
      });

      pool->publish("","test","msg number two")
      .then([&pool]()
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
         tx.publish("","test","msg number three");
         return tx.commit();
      })      
      .then([]()
      {
          std::cout << "Published" << std::endl;
      });
    });

    RabbitListener listener(pool);
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

    theLoop().run();
  }

  MOL_TEST_PRINT_CNTS();

}


int main(int argc, char **argv) 
{
    prio::Libraries<prio::EventLoop> init;

    ::testing::InitGoogleTest(&argc, argv);
    int r = RUN_ALL_TESTS();

    return r;
}

