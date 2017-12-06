#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "Logger/Logger.h"
#include "Logger/LogAppender.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/Communicator.h"
#include "SimpleHttpClient/Callbacks.h"
#include "SimpleHttpClient/Destination.h"

using namespace arangodb;
using namespace arangodb::communicator;

int main() {
  arangodb::Logger::initialize(false);
  arangodb::Logger::setLogLevel("communication=trace");
  arangodb::LogAppender::addAppender("-");
  Communicator communicator;

  std::string body("{\"test\": \"test\"}");
  auto start = std::chrono::high_resolution_clock::now();

  int numRequests = 1000;

  std::mutex m;
  std::condition_variable cond_var;
  bool notified = false;
  std::function<void()> addRequest = [&]() {
    communicator::Callbacks callbacks([&](std::unique_ptr<GeneralResponse> response) {
      std::unique_lock<std::mutex> lock(m);
      notified = true;
      // std::cout << "NOTIFYING " << std::endl;
      cond_var.notify_one();
    }, [](int errorCode, std::unique_ptr<GeneralResponse> response) {
      std::cout << "ERRORCODE " << errorCode << std::endl;
    });
    auto request = std::unique_ptr<HttpRequest>(HttpRequest::createHttpRequest(rest::ContentType::TEXT, "", 0, {}));
    request->setRequestType(RequestType::GET);
    request->setBody(body.c_str(), body.length());
    communicator::Options opt;
    auto destination = Destination("http://suelz:8529/_api/document/test");
    communicator.addRequest(destination, std::move(request), callbacks, opt);
  };

  std::thread worker(std::bind(&Communicator::work, &communicator));
  for (int i=0;i<numRequests;i++) {
    addRequest();
    std::unique_lock<std::mutex> lock(m);
    while (!notified) {
      cond_var.wait(lock);
    }
    notified = false;
    // std::cout << "WOKE UP " << std::endl;
  }
  communicator.stop();
  worker.join();

  std::cout << "HMMM" << std::endl;
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Elapsed " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
  arangodb::Logger::flush();
  arangodb::Logger::shutdown();
}