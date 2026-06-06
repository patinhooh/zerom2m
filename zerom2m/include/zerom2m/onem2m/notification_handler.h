#pragma once
#include <zerom2m/onem2m/types/primitives.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;

class INotificationHandler
{
public:
    virtual ResponsePrimitive OnNotification(const Notification &) = 0;
};

} // namespace zerom2m::onem2m