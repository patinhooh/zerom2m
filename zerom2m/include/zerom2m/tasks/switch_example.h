#pragma once

#include <zerom2m/config/system_config.h>
#include <zerom2m/kernel/network_manager.h>
#include <zerom2m/onem2m/bindings/http/http_adapter.h>
#include <zerom2m/onem2m/notification_handler.h>
#include <zerom2m/tasks/p2p_node.h>

namespace zerom2m::tasks
{

using zerom2m::config::SystemConfig;
using zerom2m::kernel::NetworkManager;
using zerom2m::onem2m::INotificationHandler;
using zerom2m::onem2m::bindings::http::HttpAdapter;
using zerom2m::onem2m::types::Notification;
using zerom2m::onem2m::types::ResponsePrimitive;

class SwitchExample : public P2PNode
{
public:
    SwitchExample(CScheduler     *scheduler,
                  NetworkManager *net,
                  HttpAdapter    *httpAdapter,
                  SystemConfig   *config,
                  bool            initialState = false);

    virtual ~SwitchExample();

    void Run() override;

protected:
    // Sub class hooks

    virtual ResponsePrimitive OnData(const Notification &sgn) override;
    virtual ResponsePrimitive OnVerification(const Notification &sgn) override;

    // Helper methods

    void SetState(bool on);

private:
    CScheduler     *scheduler_;
    NetworkManager *net_;
    HttpAdapter    *httpAdapter_;
    SystemConfig   *config_;
    bool            state_;
    CString         cinParent_;
};

} // namespace zerom2m::tasks