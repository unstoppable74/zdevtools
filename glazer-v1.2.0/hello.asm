;;
;; Hello World sample (Glulx)
;;

glk_window_open         = 0x23
glk_set_window          = 0x2F

wintype_TextBuffer      = 3

entry_point:
            function

            local       win_id

            ; use GLK for I/O
            setiosys    2 0

            ; open the GLK window
            push        0
            push        wintype_TextBuffer
            push        0
            push        0
            push        0

            glk         glk_window_open 5 -> win_id
            jz          win_id -> .finish

            ; make this window be the current stream
            push        win_id

            glk         glk_set_window 1

            ; display the message
            streamstr   message_str

            ; according to the Glulx spec, it is safe to simply print
            ; a message then quit -- the GLK layer is responsible for
            ; showing a "press any key" prompt.
.finish:
            quit

message_str:
            latstr      "Let's go girls!!\n\n"
