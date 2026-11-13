#pragma once

// Modal "has unsaved changes" popup shown when closing a modified tab.

#include "layout_common.h"
#include "layout_state.h"
#include "layout_style.h"

namespace layout_internal {
    inline void ClosePopupButton(Clay_ElementId id, Clay_String label, CloseAction action) {
        const bool discard = action == CloseAction::Discard;
        CLAY(id,
             {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(38)},
                         .padding = {16, 16, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = Clay_PointerOver(id)
                  ? (discard ? Clay_Color{240, 200, 200, 255} : menuHover)
                  : (discard ? Clay_Color{250, 225, 225, 255} : Clay_Color{235, 235, 235, 255}),
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .border = {.color = {160, 160, 160, 255}, .width = CLAY_BORDER_ALL(1)}}) {
            CLAY_TEXT(label, {.textColor = menuText, .fontId = FONT_BODY, .fontSize = 18});
            if (Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                closeAction = action;
            }
        }
    }

    inline void UnsavedChangesPopup(const FileBase& file) {
        // Clay uses this message until rendering finishes.
        static std::string message;
        message = file.path.filename().string();
        if (message.empty()) message = "untitled.mpp";
        message += " has unsaved changes.";

        CLAY(CLAY_ID("UnsavedChangesOverlay"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(24),
                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {0, 0, 0, 120},
              .floating = {.zIndex = 10, .attachTo = CLAY_ATTACH_TO_ROOT}}) {
            CLAY(CLAY_ID("UnsavedChangesPopup"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0, 480), CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(24), .childGap = 20,
                             .layoutDirection = CLAY_TOP_TO_BOTTOM},
                  .backgroundColor = {250, 250, 250, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(8),
                  .border = {.color = {160, 160, 160, 255}, .width = CLAY_BORDER_ALL(1)}}) {
                Clay_String text = {.isStaticallyAllocated = false,
                                    .length = static_cast<int32_t>(message.size()),
                                    .chars = message.c_str()};
                CLAY_TEXT(text, {.textColor = menuText, .fontId = FONT_BODY, .fontSize = 20});
                if (!closeError.empty()) {
                    Clay_String error = {.isStaticallyAllocated = false,
                                         .length = static_cast<int32_t>(closeError.size()),
                                         .chars = closeError.c_str()};
                    CLAY_TEXT(error, {.textColor = {160, 30, 30, 255},
                                      .fontId = FONT_BODY, .fontSize = 16});
                }
                CLAY(CLAY_ID("UnsavedChangesButtons"),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .childGap = 12,
                                 .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}}}) {
                    ClosePopupButton(CLAY_ID("CancelCloseButton"),
                                     CLAY_STRING("Cancel"), CloseAction::Cancel);
                    CLAY(CLAY_ID("UnsavedChangesButtonSpacer"),
                         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1)}}}) {}
                    ClosePopupButton(CLAY_ID("SaveAndCloseButton"),
                                     CLAY_STRING("Save and Close"), CloseAction::Save);
                    ClosePopupButton(CLAY_ID("CloseAnywayButton"),
                                     CLAY_STRING("Close Anyway"), CloseAction::Discard);
                }
            }
        }
    }
}
