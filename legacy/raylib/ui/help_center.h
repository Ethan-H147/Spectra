#ifndef SPECTRA_HELP_CENTER_H
#define SPECTRA_HELP_CENTER_H

#include "ui/app_shell.h"
#include "ui/theme.h"

#include <stdbool.h>

typedef struct {
    AppPage topic;
    bool close;
} HelpCenterActions;

HelpCenterActions draw_help_center(const AppTheme *theme,
                                   Rectangle workspace,
                                   AppPage selected_topic);

#endif
