#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <nlohmann/json.h>

#include <stdio.h>
#include <optional>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <string>
#include <stdio.h>

#include "cast_json.h"
#include "capy/amqp.h"

namespace py = pybind11;
using namespace pybind11::literals;

struct FetchHandler {

    virtual void on_data(const nlohmann::json data) const {}
    virtual void on_error(int code, const std::string message) const {}
    virtual void on_success() const {}
    virtual void on_finalize() const {}

    virtual ~FetchHandler() {}

};

struct PyFetchHandler : public FetchHandler {

public:
    /* Inherit the constructors */
    using FetchHandler::FetchHandler;

    /* Trampoline (need one for each virtual function) */
    void on_data(const nlohmann::json data) const override {
        PYBIND11_OVERLOAD_PURE(
            void,            /* Return type */
            FetchHandler,    /* Parent class */
            on_data,         /* Name of function in C++ (must match Python name) */
            data             /* Argument(s) */
        );
    }

    /* Trampoline (need one for each virtual function) */
    void on_error(int code, const std::string message) const override {
        PYBIND11_OVERLOAD_PURE(
            void,            /* Return type */
            FetchHandler,    /* Parent class */
            on_error,        /* Name of function in C++ (must match Python name) */
            code,            /* Argument(s) */
            message          /* Argument(s) */
        );
    }

    /* Trampoline (need one for each virtual function) */
    void on_success() const override {
        PYBIND11_OVERLOAD_PURE(
            void,            /* Return type */
            FetchHandler,    /* Parent class */
            on_success       /* Name of function in C++ (must match Python name) */
        );
    }

    /* Trampoline (need one for each virtual function) */
    void on_finalize() const override {
        PYBIND11_OVERLOAD_PURE(
            void,            /* Return type */
            FetchHandler,    /* Parent class */
            on_finalize      /* Name of function in C++ (must match Python name) */
        );
    }
};

class PyBroker {


public:

    PyBroker(const std::string& url,
             const std::string& exchange_name="amq.topic",
             unsigned int heartbeat_timeout=capy::amqp::Broker::heartbeat_timeout):
             broker_(nullptr)
             {

        auto address = capy::amqp::Address::From(url);

        if (!address) {
            std::runtime_error(address.error().message());
        }

        auto broker = capy::amqp::Broker::Bind(
                *address,
                exchange_name,
                 (uint16_t)heartbeat_timeout,
                 [](const capy::Error& error){
                     std::runtime_error(error.message());
                    }
                 );

        if (!broker) {
            std::runtime_error(broker.error().message());
        }

        broker_ = std::make_unique<capy::amqp::Broker>(*broker);
    }

    PyBroker& run() {
         if (broker_) {
            broker_->run();
         }
         return *this;
    }

    PyBroker& fetch (const nlohmann::json& message,
                const std::string& routing_key,
                const FetchHandler* handler) {

        if (broker_) {

            broker_->fetch(message,routing_key)

            .on_data([handler](const capy::amqp::Payload &response){

                if (response)
                     std::cout << "response: ... " << response->dump(4) << std::endl;
                 else
                     std::cout << "response error: ... " << response.error() << std::endl;

                if (!handler) return;

                if (response) {
                    handler->on_data(response.value());
                }
                else
                    handler->on_error(response.error().value(), response.error().message());
            })

            .on_error([handler](const capy::Error& error){

                    if (!handler) return;

                    handler->on_error(error.value(), error.message());
            })

            .on_success([handler](){

                if (!handler) return;

                handler->on_success();
            })

            .on_finalize([handler](){

                if (!handler) return;

                handler->on_finalize();
            });
        }

        return *this;

    }

private:

    std::unique_ptr<capy::amqp::Broker> broker_;

};


static std::unique_ptr<PyBroker> py_bind(const std::string& url)
{
    if (url.empty())
    {
        throw std::runtime_error("Address is empty...");
    }

    return std::unique_ptr<PyBroker>(new PyBroker(url));
}

PYBIND11_MODULE(__capy_amqp, m) {

    m.doc() = "Capy amqp module...";

    m.def(
        "Bind",
        &py_bind,
        "Bind amqp queue",
        "url"_a
    );

    py::class_<FetchHandler, PyFetchHandler >(m, "FetchHandler")
        .def(py::init<>())
        .def("on_data", &FetchHandler::on_data)
        .def("on_error", &FetchHandler::on_error)
        .def("on_success", &FetchHandler::on_success)
        .def("on_finalize", &FetchHandler::on_finalize);

    py::class_<PyBroker> py_broker_class(m, "Broker");
    py_broker_class
        .def(py::init<std::string, std::string, unsigned int>())
        .def(py::init<std::string, std::string>())
        .def(py::init<std::string, std::string>())
        .def("run", &PyBroker::run)
        .def("fetch", &PyBroker::fetch);
        //.def_property("url", &PyBroker::get_url, &PyBroker::set_url);
}
