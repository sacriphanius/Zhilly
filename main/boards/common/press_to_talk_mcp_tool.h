#ifndef PRESS_TO_TALK_MCP_TOOL_H
#define PRESS_TO_TALK_MCP_TOOL_H

#include "mcp_server.h"
#include "settings.h"

class PressToTalkMcpTool {
private:
    bool press_to_talk_enabled_;

public:
    PressToTalkMcpTool();

    void Initialize();

    bool IsPressToTalkEnabled() const;

private:

    ReturnValue HandleSetPressToTalk(const PropertyList& properties);

    void SetPressToTalkEnabled(bool enabled);
};

#endif 