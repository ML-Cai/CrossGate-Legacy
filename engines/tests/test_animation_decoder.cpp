#include <gtest/gtest.h>
#include "cgl/assets/animation_decoder.h"
TEST(AnimationDecoder, MissingReadersFailWithoutWindowOrDevice) {
    cgl::AnimationDecoder decoder;
    EXPECT_THROW(decoder.requiredBytes({},{}),std::runtime_error);
    EXPECT_THROW(decoder.build({},{}),std::runtime_error);
    decoder.clear();decoder.clear();
}
TEST(AnimationDecoder, InvalidMetadataAndCancellationNeedNoReaders) {
    cgl::AnimationDecoder decoder;cgl::AnimationRequest request;
    request.version=cgl::CrossGateVersion::COUNT;
    EXPECT_THROW(decoder.requiredBytes(request,{}),std::runtime_error);
    request={};request.direction=uint32_t(cgl::DirectionTypes::COUNT);
    EXPECT_THROW(decoder.build(request,{}),std::runtime_error);
    std::atomic_bool cancelled=true;
    EXPECT_THROW(decoder.build({}, {}, 1, &cancelled),std::runtime_error);
}
