/*
 * Copyright (C) 2017 Martin Paljak
 */

#pragma once

// Check native messaging registration
class WebExtensionHelper {

public:
    static bool isEnabled();
    static bool setEnabled(bool enabled);
};
