// AgentLibExports.h
#pragma once

#ifdef AGENTLIB_EXPORTS
    #define AGENTLIB_API __declspec(dllexport)
#else
    #define AGENTLIB_API __declspec(dllimport)
#endif 