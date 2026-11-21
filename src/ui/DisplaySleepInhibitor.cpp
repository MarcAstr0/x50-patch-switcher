#include "ui/DisplaySleepInhibitor.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

DisplaySleepInhibitor::~DisplaySleepInhibitor()
{
    release();
}

void DisplaySleepInhibitor::acquire()
{
    if (m_active)
        return;
    m_active = true;
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
#endif
}

void DisplaySleepInhibitor::release()
{
    if (!m_active)
        return;
    m_active = false;
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
