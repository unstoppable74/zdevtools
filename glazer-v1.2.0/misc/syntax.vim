" Vim syntax file
" Language:	GLAZER
" Maintainer:	Andrew Apted  <ajapted@users.sf.net>
" Last Change:	2025 Nov 30
" Home Page:	http://gitlab.com/andwj/glazer

" quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

" identifier characters
setlocal iskeyword=@,48-57,_,.

" comments
syn region  glazer_Comment	start=/;/ keepend end=/$/ contains=glazer_CommentTodo
syn keyword glazer_CommentTodo 	contained TODO FIXME XXX

" labels
syn match glazer_Label		/\k\+:/

" assignment / jump target
syn match glazer_Assign		/->/

" strings and characters
syn region glazer_String	start=/"/ skip=/\\\\\|\\"/ end=/"/
syn region glazer_String	start=/`/ skip=/\\\\\|\\`/ end=/`/

" integers
syn match glazer_Number		/\<[+-]\?\d\+/
syn match glazer_Number		/\<[+-]\?0b[01]\+/
syn match glazer_Number		/\<[+-]\?0x\x\+/

" floating point
syn match glazer_Number		/\<[+-]\?\d\+[.]\d\+/
syn match glazer_Number		/\<[+-]\?\d\+[.]\d*[eE][+-]\?\d\+/
syn match glazer_Number		/\<[+-]\?\d\+[eE][+-]\?\d\+/
syn match glazer_Number		/\<[+-]\?0x\x\+[.]\x*[pP][+-]\?\d\+/
syn match glazer_Number		/\<[+-]\?0x\x\+[pP][+-]\?\d\+/

" directives
syn keyword glazer_Directive	include section
syn keyword glazer_Directive	global release

" sections
syn keyword glazer_Section	.text .data .bss .static

" special words
syn keyword glazer_Special	drop pop push pull hilo
syn keyword glazer_Special	rtrue rfalse fail

" built-in constants
syn keyword glazer_Constant	FALSE TRUE NULL
syn keyword glazer_Constant	NAN INFINITY STACKREF

" pseudo-instructions
syn keyword glazer_Pseudo	function func_va local
syn keyword glazer_Pseudo	db dw dd dq
syn keyword glazer_Pseudo	resb resw resd resq align
syn keyword glazer_Pseudo	latstr unistr huffstr

" glulx instructions
syn keyword glazer_Instruction	nop add sub mul div mod neg
syn keyword glazer_Instruction	bitand bitor bitxor bitnot
syn keyword glazer_Instruction	shiftl sshiftr ushiftr

syn keyword glazer_Instruction	jump jz jnz jeq jne
syn keyword glazer_Instruction	jlt jge jgt jle
syn keyword glazer_Instruction	jltu jgeu jgtu jleu
syn keyword glazer_Instruction	jumpabs

syn keyword glazer_Instruction	call return catch throw tailcall
syn keyword glazer_Instruction	copy copys copyb sexs sexb
syn keyword glazer_Instruction	aload aloads aloadb aloadbit
syn keyword glazer_Instruction	astore astores astoreb astorebit
syn keyword glazer_Instruction	stkcount stkpeek stkswap stkroll stkcopy
syn keyword glazer_Instruction	streamchar streamnum streamstr streamunichar

syn keyword glazer_Instruction	gestalt debugtrap getmemsize setmemsize
syn keyword glazer_Instruction	random setrandom quit verify restart
syn keyword glazer_Instruction	save restore saveundo restoreundo
syn keyword glazer_Instruction	protect hasundo discardundo

syn keyword glazer_Instruction	glk getiosys setiosys
syn keyword glazer_Instruction	getstringtbl setstringtbl
syn keyword glazer_Instruction	linearsearch binarysearch linkedsearch
syn keyword glazer_Instruction	callf callfi callfii callfiii
syn keyword glazer_Instruction	mzero mcopy malloc mfree
syn keyword glazer_Instruction	accelfunc accelparam

syn keyword glazer_Instruction	numtof ftonumz ftonumn
syn keyword glazer_Instruction	ceil floor
syn keyword glazer_Instruction	fadd fsub fmul fdiv fmod
syn keyword glazer_Instruction	sqrt exp log pow sin cos tan
syn keyword glazer_Instruction	asin acos atan atan2
syn keyword glazer_Instruction	jfeq jfne jflt jfle jfgt jfge
syn keyword glazer_Instruction	jisnan jisinf

syn keyword glazer_Instruction	numtod dtonumz dtonumn ftod dtof
syn keyword glazer_Instruction	dceil dfloor
syn keyword glazer_Instruction	dadd dsub dmul ddiv dmodr dmodq
syn keyword glazer_Instruction	dsqrt dexp dlog dpow dsin dcos dtan
syn keyword glazer_Instruction	dasin dacos datan datan2
syn keyword glazer_Instruction	jdeq jdne jdlt jdle jdgt jdge
syn keyword glazer_Instruction	jdisnan jdisinf

" fake instructions
syn keyword glazer_Pseudo	dcopy dload dstore
syn keyword glazer_Special	dpush dpull

" z-code instructions
syn keyword glazer_Instruct_Z	je jl jg dec_chk inc_chk jin test or
syn keyword glazer_Instruct_Z	and test_attr set_attr clear_attr store
syn keyword glazer_Instruct_Z	insert_obj loadw loadb get_prop get_prop_addr
syn keyword glazer_Instruct_Z	get_next_prop add sub mul div mod call_2s
syn keyword glazer_Instruct_Z	call_2n set_colour throw

syn keyword glazer_Instruct_Z	jz get_sibling get_child get_parent
syn keyword glazer_Instruct_Z	get_prop_len inc dec print_addr call_1s
syn keyword glazer_Instruct_Z	remove_obj print_obj ret jump print_paddr
syn keyword glazer_Instruct_Z	load not call_1n

syn keyword glazer_Instruct_Z	rtrue rfalse print print_ret nop restart
syn keyword glazer_Instruct_Z	ret_popped pop catch quit new_line
syn keyword glazer_Instruct_Z	show_status verify piracy

syn keyword glazer_Instruct_Z	call call_vs storew storeb put_prop
syn keyword glazer_Instruct_Z	read print_char print_num random push
syn keyword glazer_Instruct_Z	pull split_window set_window call_vs2
syn keyword glazer_Instruct_Z	erase_window erase_line set_cursor
syn keyword glazer_Instruct_Z	get_cursor set_text_style buffer_mode
syn keyword glazer_Instruct_Z	output_stream input_stream sound_effect
syn keyword glazer_Instruct_Z	read_char scan_table not call_vn call_vn2
syn keyword glazer_Instruct_Z	tokenise encode_text copy_table
syn keyword glazer_Instruct_Z	print_table check_arg_count

syn keyword glazer_Instruct_Z	save restore log_shift art_shift
syn keyword glazer_Instruct_Z	set_font draw_picture picture_data
syn keyword glazer_Instruct_Z	erase_picture set_margins save_undo
syn keyword glazer_Instruct_Z	restore_undo print_unicode
syn keyword glazer_Instruct_Z	check_unicode set_true_colour

syn keyword glazer_Instruct_Z	move_window window_size window_style
syn keyword glazer_Instruct_Z	get_wind_prop scroll_window pop_stack
syn keyword glazer_Instruct_Z	read_mouse mouse_window push_stack
syn keyword glazer_Instruct_Z	put_wind_prop print_form make_menu
syn keyword glazer_Instruct_Z	picture_table buffer_screen

" Define the default highlighting.
" Only when an item doesn't have highlighting yet

hi def link glazer_Comment	Comment
hi def link glazer_CommentTodo	Todo
hi def link glazer_Constant	Constant
hi def link glazer_Directive	Special
hi def link glazer_Section	Special
hi def link glazer_Special	Special

hi def link glazer_Instruction	Function
hi def link glazer_Instruct_Z	Function
hi def link glazer_Pseudo	Function
hi def link glazer_Assign	Keyword
hi def link glazer_Label	Keyword

hi def link glazer_Number	Number
hi def link glazer_NumberError	Error
hi def link glazer_NumberFloat	Float
hi def link glazer_String	String
hi def link glazer_StringError	Error
hi def link glazer_StringEscape	SpecialChar

let b:current_syntax = "glazer"

" vim:ts=8:sw=8:noexpandtab:nowrap
