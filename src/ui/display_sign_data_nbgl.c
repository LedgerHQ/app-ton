#ifdef HAVE_NBGL

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "os_io_seproxyhal.h"
#include "nbgl_use_case.h"

#include "display.h"
#include "../constants.h"
#include "../globals.h"
#include "io.h"
#include "../sw.h"
#include "action/validate.h"
#include "menu.h"
#include "hint_buffers_nbgl.h"

static nbgl_contentTagValue_t pairs[MAX_HINTS];
static nbgl_contentTagValueList_t pairList;

static void on_data_accepted() {
    ui_action_validate_sign_data(true);
    nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_menu_main);
}

static void on_data_rejected() {
    ui_action_validate_sign_data(false);
    nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_menu_main);
}

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void on_review_choice(bool confirm) {
    if (confirm) {
        on_data_accepted();
    } else {
        on_data_rejected();
    }
}

static void ui_start_review() {
    print_hints(&G_context.sign_data_info.hints, pairs);

    pairList.pairs = pairs;
    pairList.nbPairs = G_context.sign_data_info.hints.hints_count;
    pairList.smallCaseForValue = false;

    nbgl_useCaseReview(TYPE_MESSAGE,
                       &pairList,
                       &ICON_APP_HOME,
                       "Sign custom data",
                       NULL,
                       "Sign custom data",
                       on_review_choice);
}

static void on_blind_choice2(bool proceed) {
    if (proceed) {
        ui_start_review();
    } else {
        on_data_rejected();
    }
}

static void on_blind_choice1(bool back_to_safety) {
    if (back_to_safety) {
        on_data_rejected();
    } else {
        nbgl_useCaseChoice(
            NULL,
            "Blind Signing",
            "This data cannot be\nsecurely interpreted by Ledger. It might put "
            "your assets\nat risk.",
            "I accept the risk",
            "Reject transaction",
            on_blind_choice2);
    }
}

static void ui_show_blind_warning() {
    nbgl_useCaseChoice(
        &LARGE_WARNING_ICON,
        "Security risk detected",
        "It may not be safe to sign this data. To continue, you'll need to review the risk.",
        "Back to safety",
        "Review risk",
        on_blind_choice1);
}

int ui_display_sign_data() {
    if (G_context.req_type != CONFIRM_SIGN_DATA || G_context.state != STATE_PARSED) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    if (G_context.sign_data_info.is_blind) {
        ui_show_blind_warning();
    } else {
        ui_start_review();
    }

    return 0;
}

#endif
