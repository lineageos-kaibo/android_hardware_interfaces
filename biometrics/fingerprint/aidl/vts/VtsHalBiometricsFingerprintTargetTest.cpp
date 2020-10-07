/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <aidl/Gtest.h>
#include <aidl/Vintf.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnSessionCallback.h>

#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <future>

namespace aidl::android::hardware::biometrics::fingerprint {
namespace {

constexpr int kSensorId = 0;
constexpr int kUserId = 0;
constexpr auto kCallbackTimeout = std::chrono::seconds(1);

enum class SessionCallbackMethodName {
    kOnStateChanged,
};

struct SessionCallbackInvocation {
    SessionCallbackMethodName method_name;
    SessionState state;
};

class SessionCallback : public BnSessionCallback {
  public:
    explicit SessionCallback(std::promise<SessionCallbackInvocation> invocation_promise)
        : invocation_promise_(std::move(invocation_promise)) {}

    ndk::ScopedAStatus onStateChanged(int32_t /*cookie*/, SessionState state) override {
        SessionCallbackInvocation invocation = {};
        invocation.method_name = SessionCallbackMethodName::kOnStateChanged;
        invocation.state = state;
        invocation_promise_.set_value(invocation);
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onAcquired(AcquiredInfo /*info*/, int32_t /*vendorCode*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onError(Error /*error*/, int32_t /*vendorCode*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onEnrollmentProgress(int32_t /*enrollmentId*/,
                                            int32_t /*remaining*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onAuthenticationSucceeded(int32_t /*enrollmentId*/,
                                       const keymaster::HardwareAuthToken& /*hat*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onAuthenticationFailed() override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onLockoutTimed(int64_t /*durationMillis*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onLockoutPermanent() override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onLockoutCleared() override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onInteractionDetected() override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus onEnrollmentsEnumerated(
            const std::vector<int32_t>& /*enrollmentIds*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onEnrollmentsRemoved(
            const std::vector<int32_t>& /*enrollmentIds*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onAuthenticatorIdRetrieved(int64_t /*authenticatorId*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onAuthenticatorIdInvalidated() override {
        return ndk::ScopedAStatus::ok();
    }

  private:
    std::promise<SessionCallbackInvocation> invocation_promise_;
};

class Fingerprint : public testing::TestWithParam<std::string> {
  protected:
    void SetUp() override {
        AIBinder* binder = AServiceManager_waitForService(GetParam().c_str());
        ASSERT_NE(binder, nullptr);
        hal_ = IFingerprint::fromBinder(ndk::SpAIBinder(binder));
    }

    std::shared_ptr<IFingerprint> hal_;
};

TEST_P(Fingerprint, AuthenticateTest) {
    std::promise<SessionCallbackInvocation> invocation_promise;
    std::future<SessionCallbackInvocation> invocation_future = invocation_promise.get_future();
    std::shared_ptr<SessionCallback> session_cb =
            ndk::SharedRefBase::make<SessionCallback>(std::move(invocation_promise));

    std::shared_ptr<ISession> session;
    ASSERT_TRUE(hal_->createSession(kSensorId, kUserId, session_cb, &session).isOk());

    std::shared_ptr<common::ICancellationSignal> cancel_cb;
    ASSERT_TRUE(session->authenticate(0, 0, &cancel_cb).isOk());
    ASSERT_EQ(invocation_future.wait_for(kCallbackTimeout), std::future_status::ready);

    SessionCallbackInvocation invocation = invocation_future.get();
    EXPECT_EQ(invocation.method_name, SessionCallbackMethodName::kOnStateChanged);
    EXPECT_EQ(invocation.state, SessionState::AUTHENTICATING);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Fingerprint);
INSTANTIATE_TEST_SUITE_P(
        IFingerprint, Fingerprint,
        testing::ValuesIn(::android::getAidlHalInstanceNames(IFingerprint::descriptor)),
        ::android::PrintInstanceNameToString);

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    ABinderProcess_startThreadPool();
    return RUN_ALL_TESTS();
}

}  // namespace aidl::android::hardware::biometrics::fingerprint