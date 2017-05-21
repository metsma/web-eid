/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

// Make the application start at login or query status
class StartAtLoginHelper {

public:
    static bool isEnabled();
    static bool setEnabled(bool enabled);
};
