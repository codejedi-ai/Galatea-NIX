#ifndef DISPLAY_MESSAGES_H
#define DISPLAY_MESSAGES_H

#define DISPLAY_SERVER_NAME "DisplayServer"

#define DISP_MSG_CHUNK 240

typedef enum {
    DISP_MSG_WRITE       = 0,  /* client → server: raw ANSI bytes (legacy path)  */
    DISP_MSG_RENDER      = 1,  /* client → server: flush screenbuf to UART now   */
    DISP_MSG_RENDER_TREE = 2,  /* client → server: declarative UiElem tree       */
    DISP_MSG_GET_SIZE    = 3,  /* client → server: query terminal rows/cols       */
    DISP_MSG_CLEAR       = 4,  /* client → server: blank the screen + sync model  */
    DISP_MSG_SCROLL      = 5,  /* client → server: scroll terminal history (len=Δ) */
    DISP_MSG_CURSOR      = 6,  /* client → server: len 1=show / 0=hide DECTCEM cursor */
    DISP_MSG_READLINE_START = 7, /* client → server: mark input start col (after prompt) */
    DISP_MSG_ERASE_CHAR    = 8,  /* client → server: backspace one input character */
    DISP_MSG_QUERY_TERMINAL = 9, /* client → server: ESC[6n physical size (CPR)     */
    DISP_MSG_WAIT_KEY      = 10, /* client → server: block for one key (reply byte)  */
    DISP_MSG_PUT_CELL      = 11, /* client → server: patch one cell in next[]       */
    DISP_MSG_FLUSH         = 12, /* client → server: diff next[]→UART, cell[]←next[] */
    DISP_MSG_SET_SIZE      = 13, /* client → server: manual rows/cols (len, flags)   */
    DISP_MSG_SCREEN_AUTO   = 14, /* client → server: re-enable autofit (CPR query)  */
    DISP_MSG_SHELL_REGION  = 15, /* client → server: scroll region (len=top, flags=bot) */
    DISP_MSG_GOTO_CURSOR   = 16, /* client → server: move cursor (len=row, flags=col) 0-based */
    DISP_MSG_READLINE_ECHO = 17, /* client → server: echo one input byte (data[0])       */
    DISP_MSG_READLINE_END  = 18, /* client → server: finish readline (newline)           */
    DISP_MSG_GET_CURSOR    = 19, /* client → server: query g_term row/col/min_col        */
    DISP_MSG_TETRIS_BEGIN  = 20, /* client → server: open Tetris chrome (full frame)    */
    DISP_MSG_TETRIS_UPDATE = 21, /* client → server: root = DispTetrisState *         */
    DISP_MSG_TETRIS_END    = 22, /* client → server: leave Tetris mode                  */
} DispMsgType;

/* DispMsg.flags bits */
#define DISP_FLAG_CLEAR 0x01   /* RENDER_TREE: force a full repaint this frame   */

typedef struct {
    int  type;
    int  len;
    int  flags;                 /* DISP_FLAG_* (must be 0 if unused)             */
    char data[DISP_MSG_CHUNK]; /* payload for DISP_MSG_WRITE                     */
    void *root;                 /* UiElem * for DISP_MSG_RENDER_TREE              */
} DispMsg;

/* Reply payload for size queries and screen_set / screen_auto */
typedef struct {
    int rows;
    int cols;
    int manual;   /* 1 = fixed resolution; 0 = autofit to terminal */
} DispSizeReply;

/* Reply for DISP_MSG_GET_CURSOR */
typedef struct {
    int row;
    int col;
    int min_col;
} DispCursorReply;

#endif
