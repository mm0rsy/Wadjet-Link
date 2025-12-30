/// @file test_scenario_runner.cpp
/// @brief Unit tests for ScenarioRunner and report generators

#include <gtest/gtest.h>
#include <sstream>

#include "wadjet/scenario/runner.hpp"
#include "wadjet/scenario/report.hpp"
#include "wadjet/scenario/scenario_types.hpp"

using namespace wadjet::scenario;

// =============================================================================
// ScenarioResult Tests
// =============================================================================

class ScenarioResultTest : public ::testing::Test {
protected:
    ScenarioResult create_passed_result() {
        ScenarioResult result;
        result.scenario_name = "Passed Test";
        result.passed = true;
        result.total_elapsed = Duration{1500};
        
        ExpectResult expect1;
        expect1.description = "Check packet received";
        expect1.passed = true;
        expect1.packets_matched = 5;
        result.expect_results.push_back(expect1);
        
        return result;
    }
    
    ScenarioResult create_failed_result() {
        ScenarioResult result;
        result.scenario_name = "Failed Test";
        result.passed = false;
        result.total_elapsed = Duration{2000};
        
        ExpectResult expect1;
        expect1.description = "Check service offer";
        expect1.passed = true;
        expect1.packets_matched = 3;
        result.expect_results.push_back(expect1);
        
        ExpectResult expect2;
        expect2.description = "Check service subscription";
        expect2.passed = false;
        expect2.packets_matched = 0;
        result.expect_results.push_back(expect2);
        
        return result;
    }
};

TEST_F(ScenarioResultTest, PassedResultProperties) {
    auto result = create_passed_result();
    
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.scenario_name, "Passed Test");
    EXPECT_EQ(result.total_elapsed, Duration{1500});
    EXPECT_EQ(result.expect_results.size(), 1);
}

TEST_F(ScenarioResultTest, FailedResultProperties) {
    auto result = create_failed_result();
    
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.expect_results.size(), 2);
    EXPECT_TRUE(result.expect_results[0].passed);
    EXPECT_FALSE(result.expect_results[1].passed);
}

// =============================================================================
// BatchResult Tests
// =============================================================================

class BatchResultTest : public ::testing::Test {
protected:
    BatchResult create_batch_result() {
        BatchResult batch;
        batch.passed = 3;
        batch.failed = 2;
        batch.total_elapsed = Duration{10000};
        return batch;
    }
};

TEST_F(BatchResultTest, BatchStatistics) {
    auto batch = create_batch_result();
    
    EXPECT_EQ(batch.total(), 5);
    EXPECT_EQ(batch.passed, 3);
    EXPECT_EQ(batch.failed, 2);
}

TEST_F(BatchResultTest, AllPassed) {
    BatchResult batch;
    batch.passed = 5;
    batch.failed = 0;
    
    EXPECT_TRUE(batch.all_passed());
}

TEST_F(BatchResultTest, NotAllPassed) {
    BatchResult batch;
    batch.passed = 4;
    batch.failed = 1;
    
    EXPECT_FALSE(batch.all_passed());
}

// =============================================================================
// JUnit XML Report Generator Tests
// =============================================================================

class JUnitXmlReportTest : public ScenarioResultTest {};

TEST_F(JUnitXmlReportTest, GenerateEmptyReport) {
    JUnitXmlReportGenerator generator;
    BatchResult batch;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("<?xml version=\"1.0\"") != std::string::npos);
    EXPECT_TRUE(result.find("<testsuites") != std::string::npos);
    EXPECT_TRUE(result.find("</testsuites>") != std::string::npos);
}

TEST_F(JUnitXmlReportTest, GeneratePassedTestCase) {
    JUnitXmlReportGenerator generator;
    BatchResult batch;
    batch.results.push_back(create_passed_result());
    batch.passed = 1;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("<testsuite") != std::string::npos);
    EXPECT_TRUE(result.find("name=\"Passed Test\"") != std::string::npos);
}

TEST_F(JUnitXmlReportTest, GenerateFailedTestCase) {
    JUnitXmlReportGenerator generator;
    BatchResult batch;
    batch.results.push_back(create_failed_result());
    batch.failed = 1;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("<testsuite") != std::string::npos);
}

// =============================================================================
// JSON Report Generator Tests
// =============================================================================

class JsonReportTest : public ScenarioResultTest {};

TEST_F(JsonReportTest, GenerateEmptyReport) {
    JsonReportGenerator generator;
    BatchResult batch;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("\"summary\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"scenarios\"") != std::string::npos);
}

TEST_F(JsonReportTest, GenerateWithResults) {
    JsonReportGenerator generator;
    BatchResult batch;
    batch.results.push_back(create_passed_result());
    batch.results.push_back(create_failed_result());
    batch.passed = 1;
    batch.failed = 1;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("\"passed\": true") != std::string::npos);
    EXPECT_TRUE(result.find("\"passed\": false") != std::string::npos);
    EXPECT_TRUE(result.find("\"Passed Test\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"Failed Test\"") != std::string::npos);
}

// =============================================================================
// Text Report Generator Tests
// =============================================================================

class TextReportTest : public ScenarioResultTest {};

TEST_F(TextReportTest, GenerateEmptyReport) {
    TextReportGenerator generator;
    BatchResult batch;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("Test Results") != std::string::npos);
    EXPECT_TRUE(result.find("Total: 0") != std::string::npos);
}

TEST_F(TextReportTest, GenerateWithPassedTest) {
    TextReportGenerator generator;
    BatchResult batch;
    batch.results.push_back(create_passed_result());
    batch.passed = 1;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("PASSED") != std::string::npos || 
                result.find("✓") != std::string::npos ||
                result.find("[PASS]") != std::string::npos);
    EXPECT_TRUE(result.find("Passed Test") != std::string::npos);
}

// =============================================================================
// TAP Report Generator Tests
// =============================================================================

class TapReportTest : public ScenarioResultTest {};

TEST_F(TapReportTest, GenerateEmptyReport) {
    TapReportGenerator generator;
    BatchResult batch;
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    EXPECT_TRUE(result.find("TAP version") != std::string::npos);
    EXPECT_TRUE(result.find("1..0") != std::string::npos);
}

TEST_F(TapReportTest, GenerateWithTests) {
    TapReportGenerator generator;
    BatchResult batch;
    batch.results.push_back(create_passed_result());
    batch.results.push_back(create_failed_result());
    
    std::ostringstream output;
    generator.generate(output, batch);
    
    std::string result = output.str();
    // Passed has 1 expect, Failed has 2 expects = 3 total tests
    EXPECT_TRUE(result.find("1..3") != std::string::npos);
    EXPECT_TRUE(result.find("ok 1") != std::string::npos);
    // Test 3 is the failed expectation
    EXPECT_TRUE(result.find("not ok 3") != std::string::npos);
}

// =============================================================================
// Runner Options Tests
// =============================================================================

class RunnerOptionsTest : public ::testing::Test {};

TEST_F(RunnerOptionsTest, DefaultOptions) {
    RunnerOptions options;
    
    EXPECT_FALSE(options.verbose);
    EXPECT_FALSE(options.dry_run);
    EXPECT_FALSE(options.stop_on_first_failure);
    EXPECT_TRUE(options.save_pcap_on_failure);
}

TEST_F(RunnerOptionsTest, CustomOptions) {
    RunnerOptions options;
    options.verbose = true;
    options.dry_run = true;
    options.stop_on_first_failure = true;
    options.save_pcap_on_failure = false;
    options.pcap_output_dir = "/tmp/pcaps";
    options.global_timeout = Duration{60000};
    
    EXPECT_TRUE(options.verbose);
    EXPECT_TRUE(options.dry_run);
    EXPECT_TRUE(options.stop_on_first_failure);
    EXPECT_FALSE(options.save_pcap_on_failure);
    EXPECT_EQ(options.pcap_output_dir, "/tmp/pcaps");
    EXPECT_EQ(options.global_timeout, Duration{60000});
}

// =============================================================================
// Report Factory Tests
// =============================================================================

class ReportFactoryTest : public ::testing::Test {};

TEST_F(ReportFactoryTest, CreateJUnitGenerator) {
    auto generator = create_report_generator(ReportFormat::JUnitXML);
    EXPECT_NE(generator, nullptr);
}

TEST_F(ReportFactoryTest, CreateJsonGenerator) {
    auto generator = create_report_generator(ReportFormat::JSON);
    EXPECT_NE(generator, nullptr);
}

TEST_F(ReportFactoryTest, CreateTextGenerator) {
    auto generator = create_report_generator(ReportFormat::Text);
    EXPECT_NE(generator, nullptr);
}

TEST_F(ReportFactoryTest, CreateTapGenerator) {
    auto generator = create_report_generator(ReportFormat::TAP);
    EXPECT_NE(generator, nullptr);
}
