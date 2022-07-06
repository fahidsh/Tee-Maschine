//
// Created by fahid on 28.06.2022.
//


#include <cstdarg>
#include <cstdio>
#include "serial-debugger.h"


void MbedSerialDebugger::log(const char *message_with_args, ...) {
    va_list args;
    va_start(args, message_with_args);
    char message[250];
    vsprintf(message, message_with_args, args);
    va_end(args);
    print_message(MessageType::INFO, message);
}

void MbedSerialDebugger::log(MessageType type, const char *message_with_args, ...) {
    va_list args;
    va_start(args, message_with_args);
    char message[250];
    vsprintf(message, message_with_args, args);
    va_end(args);
    print_message(type, message);

}

void MbedSerialDebugger::print_message(MessageType type, const char *message) {

    switch(type) {
        case MessageType::INFO:
            printf("[INFO]\t %s\n", message);
            break;
        case MessageType::DEBUG:
            printf("[DEBUG]\t %s\n", message);
            break;
        case MessageType::LOG:
            printf("[LOG]\t %s\n", message);
            break;
        case MessageType::WARNING:
            printf("[WARN.]\t %s\n", message);
            break;
        case MessageType::ERROR:
            printf("[ERROR]\t %s\n", message);
            break;
        case MessageType::FETAL:
            printf("[FETAL]\t %s\n", message);
            break;
    }
}
