#pragma once

#include <QStringList>

// Port enumeration for now; T1.7 grows this into the full backend.
class RtMidiBackend
{
public:
    struct Ports
    {
        QStringList outputs;
        QStringList inputs;
        QString error; // empty when enumeration succeeded
    };

    // Never throws: RtMidiError is caught and reported in Ports::error.
    static Ports listPorts();
};
