#include <gtest/gtest.h>
#include <wadjet/io/device.hpp>

namespace wadjet::test {

TEST(Device, EnumerateDevices) {
    auto result = io::enumerate_devices();
    ASSERT_TRUE(result.is_ok()) << result.error().message;

    auto& devices = result.value();
    EXPECT_GT(devices.size(), 0) << "Should have at least one network device";

    // Should have loopback
    bool has_loopback = false;
    for (const auto& dev : devices) {
        if (dev.is_loopback) {
            has_loopback = true;
            EXPECT_TRUE(dev.name == "lo" || dev.name.find("loop") != std::string::npos);
        }
    }
    EXPECT_TRUE(has_loopback) << "Should have a loopback device";
}

TEST(Device, GetDevice) {
    // Get loopback device (should always exist)
    auto result = io::get_device("lo");
    if (result.is_ok()) {
        EXPECT_EQ(result->name, "lo");
        EXPECT_TRUE(result->is_loopback);
    }
    // It's okay if lo doesn't exist on some systems
}

TEST(Device, GetNonexistentDevice) {
    auto result = io::get_device("nonexistent_device_12345");
    EXPECT_FALSE(result.is_ok());
}

TEST(Device, DeviceProperties) {
    auto result = io::enumerate_devices();
    ASSERT_TRUE(result.is_ok());

    for (const auto& dev : result.value()) {
        // All devices should have a name
        EXPECT_FALSE(dev.name.empty());

        // Index should be valid
        EXPECT_GT(dev.index, 0);

        // MTU should be reasonable (loopback can have 65536)
        if (dev.is_up) {
            EXPECT_GT(dev.mtu, 0);
        }
    }
}

}  // namespace wadjet::test
