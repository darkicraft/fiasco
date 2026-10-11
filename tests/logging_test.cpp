#include <catch2/catch_test_macros.hpp>

#include "fiasco/internal/logging/log_registry.hpp"
#include "thread_safe_sink.hpp"

using namespace fiasco::detail;

// ---- RAII guard: redirects log output to a stringstream ----------

struct RegistryGuard {
    fiasco_test::thread_safe_sink captured;
    log_registry* reg;

    RegistryGuard()
        : reg(&log_registry::instance()) {
        reg->set_sink(captured.stream());
    }

    ~RegistryGuard() {
        reg->flush();  // drain before the sink is torn down
        reg->reset_sink();
        reg->set_level(log_level::info);
    }

    // Flush (wait for the consumer), then snapshot -- deterministic, no sleeping.
    std::string output() const {
        reg->flush();
        return captured.str();
    }
};

// ============================================================
// log_level
// ============================================================

TEST_CASE("log_level to_string", "[logging][level]") {
    REQUIRE(std::string(to_string(log_level::debug)) == "debug");
    REQUIRE(std::string(to_string(log_level::info)) == "info");
    REQUIRE(std::string(to_string(log_level::warn)) == "warn");
    REQUIRE(std::string(to_string(log_level::error)) == "error");
}

TEST_CASE("log_level from_string", "[logging][level]") {
    REQUIRE(log_level_from_string("debug") == log_level::debug);
    REQUIRE(log_level_from_string("info") == log_level::info);
    REQUIRE(log_level_from_string("warn") == log_level::warn);
    REQUIRE(log_level_from_string("error") == log_level::error);
    REQUIRE(log_level_from_string("0") == log_level::debug);
    REQUIRE(log_level_from_string("1") == log_level::info);
    REQUIRE(log_level_from_string("2") == log_level::warn);
    REQUIRE(log_level_from_string("3") == log_level::error);
}

TEST_CASE("log_level from_string throws on bad input", "[logging][level]") {
    REQUIRE_THROWS_AS(log_level_from_string("invalid"), std::invalid_argument);
}

// ============================================================
// log_queue
// ============================================================

TEST_CASE("log_queue starts empty", "[logging][queue]") {
    log_queue q;
    REQUIRE(q.size() == 0);
    REQUIRE(q.try_dequeue() == std::nullopt);
}

TEST_CASE("log_queue enqueue and dequeue", "[logging][queue]") {
    log_queue q;
    log_record r;
    r.logger_name = "test";
    r.message = "hello";

    q.enqueue(std::move(r));
    REQUIRE(q.size() == 1);

    auto out = q.try_dequeue();
    REQUIRE(out.has_value());
    REQUIRE(out->logger_name == "test");
    REQUIRE(out->message == "hello");
    REQUIRE(q.size() == 0);
}

TEST_CASE("log_queue multiple records maintain order", "[logging][queue]") {
    log_queue q;
    for (int i = 0; i < 10; ++i) {
        log_record r;
        r.logger_name = "test";
        r.message = std::to_string(i);
        q.enqueue(std::move(r));
    }
    REQUIRE(q.size() == 10);
    for (int i = 0; i < 10; ++i) {
        auto out = q.try_dequeue();
        REQUIRE(out.has_value());
        REQUIRE(out->message == std::to_string(i));
    }
    REQUIRE(q.size() == 0);
}

TEST_CASE("log_queue stop wakes consumer", "[logging][queue]") {
    log_queue q;
    q.stop();
    auto r = q.dequeue_wait();
    REQUIRE(r.logger_name.empty());
}

// ============================================================
// log_formatter
// ============================================================

TEST_CASE("human formatter plain output", "[logging][formatter]") {
    human_log_formatter fmt(false);

    log_record r;
    r.level = log_level::info;
    r.logger_name = "test";
    r.message = "hello world";
    r.timestamp = std::chrono::system_clock::from_time_t(0);

    auto out = fmt.format(r);
    REQUIRE(out.find("info") != std::string::npos);
    REQUIRE(out.find("test") != std::string::npos);
    REQUIRE(out.find("hello world") != std::string::npos);
    REQUIRE(out.back() == '\n');
}

TEST_CASE("human formatter includes fields", "[logging][formatter]") {
    human_log_formatter fmt(false);

    log_record r;
    r.level = log_level::warn;
    r.logger_name = "app";
    r.message = "something";
    r.fields["key"] = "val";
    r.fields["count"] = "42";
    r.timestamp = std::chrono::system_clock::from_time_t(0);

    auto out = fmt.format(r);
    REQUIRE(out.find("key=val") != std::string::npos);
    REQUIRE(out.find("count=42") != std::string::npos);
}

TEST_CASE("json formatter produces valid JSON", "[logging][formatter]") {
    json_log_formatter fmt;

    log_record r;
    r.level = log_level::error;
    r.logger_name = "app.err";
    r.message = "fail";
    r.fields["code"] = "500";
    r.timestamp = std::chrono::system_clock::from_time_t(0);

    auto out = fmt.format(r);
    REQUIRE(out.find(R"("level":"error")") != std::string::npos);
    REQUIRE(out.find(R"("logger":"app.err")") != std::string::npos);
    REQUIRE(out.find(R"("message":"fail")") != std::string::npos);
    REQUIRE(out.find(R"("code":"500")") != std::string::npos);
    REQUIRE(out.back() == '\n');
}

// ============================================================
// log_registry + logger (via registry)
// ============================================================

TEST_CASE("registry get creates loggers", "[logging][registry]") {
    RegistryGuard guard;
    auto& log = guard.reg->get("myapp");
    REQUIRE(log.name() == "myapp");

    auto& log2 = guard.reg->get("myapp");
    REQUIRE(&log == &log2);
}

TEST_CASE("logger level filtering", "[logging][logger]") {
    RegistryGuard guard;
    auto& log = guard.reg->get("level_test");
    log.set_level(log_level::warn);

    log.debug("should not appear");
    log.info("should not appear either");
    log.warn("warning message");
    log.error("error message");

    auto out = guard.output();
    REQUIRE(out.find("should not appear") == std::string::npos);
    REQUIRE(out.find("warning message") != std::string::npos);
    REQUIRE(out.find("error message") != std::string::npos);
}

TEST_CASE("logger set_level dynamic", "[logging][logger]") {
    RegistryGuard guard;
    auto& log = guard.reg->get("dynamic_test");
    log.set_level(log_level::error);

    log.info("filtered");
    REQUIRE(guard.output().find("filtered") == std::string::npos);

    log.set_level(log_level::debug);
    log.debug("now visible");
    REQUIRE(guard.output().find("now visible") != std::string::npos);
}

TEST_CASE("logger name is preserved", "[logging][logger]") {
    RegistryGuard guard;
    auto& log = guard.reg->get("my.logger");
    REQUIRE(log.name() == "my.logger");
}

TEST_CASE("registry access log toggle", "[logging][registry]") {
    auto& reg = log_registry::instance();
    reg.enable_access_log(false);
    REQUIRE_FALSE(reg.access_log_enabled());
    reg.enable_access_log(true);
    REQUIRE(reg.access_log_enabled());
}

TEST_CASE("registry level affects new loggers", "[logging][registry]") {
    RegistryGuard guard;
    guard.reg->set_level(log_level::warn);

    auto& log = guard.reg->get("new_logger");
    REQUIRE(log.level() == log_level::warn);
}

TEST_CASE("registry log output appears via sink", "[logging][registry]") {
    RegistryGuard guard;
    guard.reg->set_level(log_level::debug);

    auto& log = guard.reg->get("test_logger");
    log.info("test message");

    auto out = guard.output();
    REQUIRE(out.find("test message") != std::string::npos);
    REQUIRE(out.find("test_logger") != std::string::npos);
}
