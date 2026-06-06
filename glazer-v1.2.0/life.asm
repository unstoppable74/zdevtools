;;
;; Conway's game of life
;;

; this board size gives a long sequence for the R-pentomino
W = 75
H = 21

; GLK stuff...
glk_window_open         = 0x23
glk_window_get_size     = 0x25
glk_window_move_cursor  = 0x2B
glk_set_window          = 0x2F
glk_select              = 0xC0
glk_request_timer_event = 0xD6

evtype_Timer            = 1
wintype_TextGrid        = 4

section     .text

entry_point:
            function

            call        init_screen 0
            call        r_pentomino 0
.loop:
            call        swap_boards   0
            call        display_board 0
            call        flush_screen  0
            call        compute_board 0

            jump        .loop


swap_boards:
            function

            copy        [CUR]  -> push
            copy        [PREV] -> [CUR]
            copy        pop    -> [PREV]

            return


r_pentomino:
            function

            push        1
            push        11
            push        31
            call        set_cell 3

            push        1
            push        12
            push        31
            call        set_cell 3

            push        1
            push        13
            push        31
            call        set_cell 3

            push        1
            push        11
            push        32
            call        set_cell 3

            push        1
            push        12
            push        30
            call        set_cell 3

            return


display_board:
            function

            local       y
            copy        0 -> y

.loop:
            jge         y H -> rfalse
            jge         y [height] -> rfalse

            push        y
            call        render_row 1

            add         y 1 -> y
            jump        .loop


render_row:
            function
            local       y

            ; move cursor to start of row
            push        y
            push        0
            push        [win_id]
            glk         glk_window_move_cursor 3

            local       x
            copy        0 -> x
.loop:
            jge         x W -> rfalse
            jge         x [width] -> rfalse
            
            push        y
            push        x
            call        render_cell 2

            add         x 1 -> x
            jump        .loop


render_cell:
            function
            local       x y
            local       val

            push        y
            push        x
            call        get_cell 2 -> val

            jz          val -> .dead
.alive:
            streamchar  `#`
            return
.dead:
            streamchar  `.`
            return


compute_board:
            function
            local       y

            copy        0 -> y
.loop:
            jge         y H -> rfalse

            push        y
            call        compute_row 1

            add         y 1 -> y
            jump        .loop


compute_row:
            function
            local       y
            local       x
.loop:
            jge         x W -> rfalse

            push        y
            push        x
            call        compute_cell 2

            add         x 1 -> x
            jump        .loop


compute_cell:
            function
            local       x y

            local       middle
            local       neighbors

            push        y
            push        x
            call        get_cell 2 -> middle

            push        y
            push        x
            call        count_neighbors 2 -> neighbors

            local       new_cell
            copy        0 -> new_cell

            jeq         neighbors 3 -> .alive
            jz          middle      -> .dead
            jne         neighbors 2 -> .dead
.alive:     
            copy        1 -> new_cell
.dead:
            push        new_cell
            push        y
            push        x
            call        set_cell 3

            return


count_neighbors:
            function
            local       mx my

            ; l/m/h prefixes represent: low/mid/high
            local       lx ly
            local       hx hy

            push        -1
            push        mx
            call        wrap_x 2 -> lx

            push        +1
            push        mx
            call        wrap_x 2 -> hx

            push        -1
            push        my
            call        wrap_y 2 -> ly

            push        +1
            push        my
            call        wrap_y 2 -> hy

            local       total
            local       val

            ; the top row...
            push        ly
            push        lx
            call        get_cell 2 -> val
            add         total val -> total

            push        ly
            push        mx
            call        get_cell 2 -> val
            add         total val -> total

            push        ly
            push        hx
            call        get_cell 2 -> val
            add         total val -> total

            ; the middle row...
            push        my
            push        lx
            call        get_cell 2 -> val
            add         total val -> total

            push        my
            push        hx
            call        get_cell 2 -> val
            add         total val -> total

            ; the bottom row...
            push        hy
            push        lx
            call        get_cell 2 -> val
            add         total val -> total

            push        hy
            push        mx
            call        get_cell 2 -> val
            add         total val -> total

            push        hy
            push        hx
            call        get_cell 2 -> val
            add         total val -> total

            return      total


            ; NOTE:
            ;      set_cell stores into the current board.
            ;      get_cell loads from the previous board.
set_cell:
            function
            local       x y val
            local       ofs

            mul         y W   -> ofs
            add         x ofs -> ofs

            astoreb     [CUR] ofs val
            return      0


get_cell:
            function
            local       x y val
            local       ofs

            mul         y W   -> ofs
            add         x ofs -> ofs

            aloadb      [PREV] ofs -> val
            return      val


wrap_x:
            function
            local       x dx

            add         x dx -> x

            jlt         x 0 -> .under
            jge         x W -> .over
            return      x
.under:
            add         x W -> x
            return      x
.over:
            sub         x W -> x
            return      x


wrap_y:
            function
            local       y dy

            add         y dy -> y

            jlt         y 0 -> .under
            jge         y H -> .over
            return      y
.under:
            add         y H -> y
            return      y
.over:
            sub         y H -> y
            return      y


init_screen:
            function

            ; use GLK for I/O
            setiosys    2 0

            ; open the GLK window
            push        0
            push        wintype_TextGrid
            push        0
            push        0
            push        0
            glk         glk_window_open 5 -> [win_id]

            jz          [win_id] -> .error

            ; make this window be the current stream
            push        [win_id]
            glk         glk_set_window 1

            ; get the window size
            push        height
            push        width
            push        [win_id]
            glk         glk_window_get_size 3

            ; we want the flush function to have a small delay,
            ; hence use timer events to achieve that.
            push        50
            glk         glk_request_timer_event 1

            return
.error:
            quit


flush_screen:
            function
.loop:
            push        event
            glk         glk_select 1

            local       type
            aload       event 0 -> type

            ; ignore all events except the time-out
            jne         type evtype_Timer -> .loop

            return


section     .data

            ; pointers to the current and previous board.
            ; these are swapped after each iteration.

CUR:        dd          BOARD_1
PREV:       dd          BOARD_2

BOARD_1:    resb        (W * H)
BOARD_2:    resb        (W * H)

            ; I/O variables

win_id:     resd        1
width:      resd        1
height:     resd        1
event:      resd        4
