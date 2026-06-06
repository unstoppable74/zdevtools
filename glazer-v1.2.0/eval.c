// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

#include "main.h"

#define UNARY_INTEGER(prec)  ((prec) & 1)
#define UNARY_FLOAT(prec)    ((prec) & 2)

#define BINARY_FLOAT(prec)  (((prec) == 1) || ((prec) == 4) || ((prec) == 6))

int IsUnaryOperator (struct Node * T)
{
	// returns zero when not a unary operator.
	// returns 1 when usable only on integers.
	// returns 2 when usable only on floating point.
	// returns 3 when usable for both.

	if (Match (T, "~")) return 1;
	if (Match (T, "+")) return 3;
	if (Match (T, "-")) return 3;

	return 0;
}

int IsBinaryOperator (struct Node * T)
{
	// returns a precedence number (higher means it binds more strongly).
	// zero means not a binary operator.
	//
	// these precedences are carefully chosen so that 1,4,6 can be
	// used with floating point (the others require integers).

/* WISH: the exponent operator
	if (Match (T, "**"))  return 6;
*/
	if (Match (T, "<<"))  return 5;
	if (Match (T, ">>"))  return 5;
	if (Match (T, ">>>")) return 5;

	if (Match (T, "*")) return 4;
	if (Match (T, "/")) return 4;
	if (Match (T, "%")) return 4;

	if (Match (T, "&")) return 3;
	if (Match (T, "^")) return 3;
	if (Match (T, "|")) return 2;

	if (Match (T, "+")) return 1;
	if (Match (T, "-")) return 1;

	return 0;
}

//----------------------------------------------------------------------

// process expressions in the node (which is usually an instruction,
// such as NI_Opcode or ND_Data).
//
// this not only handles expressions, but also converts identifiers
// into more appropriate nodes (NI_Label for labels, NX_Local for local
// variables, NX_Global for global vars, and replaces constants with
// their computed value).
//
// we rebuild the children in the node, since some of them may need
// to be replaced by new nodes.

bool ExpressionsInNode (struct Node * U, enum EvalContext ctx, struct FunctionDef * cur_func)
{
	struct Node * children = U->children;

	Node_Clear (U);

	while (children != NULL)
	{
		struct Node * T = children;
		children = children->next;

		SetErrorPos (T->pos);

		struct Node * V;

		switch (T->kind)
		{
			case NX_Memory:
			case NX_JumpDest:
			case NX_JumpFail:
			case NS_Complex:
			case NS_Indirect:
				// recurse into certain operand-ish nodes, but avoid the
				// expression-ish nodes (NX_Binary, NX_Address, etc).
				if (! ExpressionsInNode (T, ctx, cur_func))
					return false;
				break;

			case NL_Word:
			case NX_Unary:
			case NX_Binary:
			case NX_Address:
				V = Evaluate (T, ctx, cur_func);

				if (V == P_FAIL)
					return false;

				// wrap label expressions into a NX_Address node.
				// this will only occur during the resolving pass.
				if (V == P_UNKNOWN && T->kind != NX_Address)
				{
					V = Node_New (NX_Address, "", T->pos);
					V->num = -1;
					Node_Add (V, T);
				}

				if (V != P_UNKNOWN)
					T = V;
				break;

			default:
				break;
		}

		Node_Add (U, T);
	}

	// okay!
	return true;
}

//----------------------------------------------------------------------

struct Node * Eval_IntegerToFloat (struct Node * T)
{
	struct Node * U = Node_New (NL_Float, "", T->pos);

	U->kind = NL_Float;
	U->num  = 0;
	U->data.flt = (double)T->num;

	return U;
}

struct Node * Eval_MakeNAN (void)
{
	return Node_New (NL_FltSpec, "NAN", no_position);
}

struct Node * Eval_MakeInfinity (bool negative)
{
	return Node_New (NL_FltSpec, negative ? "-INFINITY" : "+INFINITY", no_position);
}

//----------------------------------------------------------------------

struct Node * Eval_Float_Unary (const char * name, struct Node * term)
{
	if (term->kind == NL_FltSpec)
	{
		const char * str = Node_Str (term);

		if (str[0] == '+')
			return Eval_MakeInfinity (name[0] == '-');
		else if (str[0] == '-')
			return Eval_MakeInfinity (name[0] != '-');
		else
			return Eval_MakeNAN ();
	}

	struct Node * result = Node_New (NL_Float, "", term->pos);

	if (strcmp (name, "+") == 0)
		result->data.flt = + term->data.flt;
	else if (strcmp (name, "-") == 0)
		result->data.flt = - term->data.flt;
	else
		Fatal_Error ("BUG: unimplemented unary operator '%s'\n", name);

	return result;
}

struct Node * Eval_Unary (struct Node * expr, enum EvalContext ctx, struct FunctionDef * cur_func)
{
	struct Node * term = Evaluate (expr->children, ctx, cur_func);

	if (term == P_FAIL || term == P_UNKNOWN)
		return term;

	const char * name = Node_Str (expr);

	// get the types of the arguments, check they are suitable
	bool is_integer = false;
	bool is_float   = false;
	bool is_address = false;

	switch (term->kind)
	{
		case NL_Integer:
		case NL_Char:
			is_integer = true;
			break;

		case NX_Address:
			is_integer = is_address = true;
			break;

		case NL_Float:
		case NL_FltSpec:
			is_float = true;
			break;

		default:
			PostError ("weird value for '%s' operator", name);
			return P_FAIL;
	}

	if (is_integer & ! UNARY_INTEGER (expr->num))
	{
		PostError ("cannot use the '%s' operator with integers", name);
		return P_FAIL;
	}
	else if (is_float & ! UNARY_FLOAT (expr->num))
	{
		PostError ("cannot use the '%s' operator with floats", name);
		return P_FAIL;
	}

	if (is_float)
		return Eval_Float_Unary (name, term);

	struct Node * result = Node_New (is_address ? NX_Address : NL_Integer, "", expr->pos);

	if (strcmp (name, "~") == 0)
		result->num = ~ term->num;
	else if (strcmp (name, "+") == 0)
		result->num = + term->num;
	else if (strcmp (name, "-") == 0)
		result->num = - term->num;
	else
		Fatal_Error ("BUG: unimplemented unary operator '%s'\n", name);

	return result;
}

//----------------------------------------------------------------------

int Eval_CategorizeFloat (struct Node * term)
{
	if (term->kind == NL_FltSpec)
	{
		const char * str = Node_Str (term);

		if (str[0] == '+' || str[0] == '-')
			return FP_INFINITE;
		else
			return FP_NAN;
	}
	else
	{
		if (fpclassify (term->data.flt) == FP_ZERO)
			return FP_ZERO;
		else
			return FP_NORMAL;
	}
}

double Eval_GetFloatValue (struct Node * term, int category)
{
	Assert (category != FP_NAN);

	if (category == FP_INFINITE)
	{
		const char * str = Node_Str (term);
		if (str[0] == '-')
			return -INFINITY;
		else
			return  INFINITY;
	}

	return term->data.flt;
}

struct Node * Eval_Float_Binary (const char * name, struct Node * lhs, struct Node * rhs)
{
	// categorize each side
	int L_cat = Eval_CategorizeFloat (lhs);
	int R_cat = Eval_CategorizeFloat (rhs);

	// doing anything with a NAN produces a NAN
	if (L_cat == FP_NAN || R_cat == FP_NAN)
		return Eval_MakeNAN ();

	double L_val = Eval_GetFloatValue (lhs, L_cat);
	double R_val = Eval_GetFloatValue (rhs, R_cat);

	double value = 0;

	if (strcmp (name, "+") == 0)
	{
		value = L_val + R_val;
	}
	else if (strcmp (name, "-") == 0)
	{
		value = L_val - R_val;
	}
	else if (strcmp (name, "*") == 0)
	{
		value = L_val * R_val;
	}
	else if (strcmp (name, "/") == 0)
	{
		value = L_val / R_val;
	}
	else if (strcmp (name, "%") == 0)
	{
		value = fmod (L_val, R_val);
	}
	else
	{
		Fatal_Error ("BUG: unimplemented operator '%s'\n", name);
	}

	// is the result is a special floating point value?
	int cat = fpclassify (value);

	if (cat == FP_NAN)
		return Eval_MakeNAN ();

	if (cat == FP_INFINITE)
		return Eval_MakeInfinity (value < 0);

	struct Node * result = Node_New (NL_Float, "", lhs->pos);
	result->data.flt = value;

	return result;
}

struct Node * Eval_Binary (struct Node * expr, enum EvalContext ctx, struct FunctionDef * cur_func)
{
	struct Node * lhs = Evaluate (expr->children,       ctx, cur_func);
	struct Node * rhs = Evaluate (expr->children->next, ctx, cur_func);

	if (lhs == P_FAIL || lhs == P_UNKNOWN) return lhs;
	if (rhs == P_FAIL || rhs == P_UNKNOWN) return rhs;

	const char * name = Node_Str (expr);

	// get the types of the arguments, check they are suitable
	bool L_float = false;
	bool R_float = false;
	bool is_address = false;

	switch (lhs->kind)
	{
		case NL_Integer:
		case NL_Char:
			// okay
			break;

		case NX_Address:
			is_address = true;
			break;

		case NL_Float:
		case NL_FltSpec:
			L_float = true;
			break;

		default:
			PostError ("weird value for '%s' operator", name);
			return P_FAIL;
	}

	switch (rhs->kind)
	{
		case NL_Integer:
		case NL_Char:
			// okay
			break;

		case NX_Address:
			is_address = true;
			break;

		case NL_Float:
		case NL_FltSpec:
			R_float = true;
			break;

		default:
			PostError ("weird value for '%s' operator", name);
			return P_FAIL;
	}

	// when only one is a float, convert other to a float
	if (L_float != R_float)
	{
		if (L_float)
			rhs = Eval_IntegerToFloat (rhs);
		else
			lhs = Eval_IntegerToFloat (lhs);

		L_float = R_float = true;
	}

	if (L_float && ! BINARY_FLOAT (expr->num))
	{
		PostError ("cannot use the '%s' operator with floats", name);
		return P_FAIL;
	}

	// handle floating point operators
	if (L_float)
		return Eval_Float_Binary (name, lhs, rhs);

	struct Node * result = Node_New (is_address ? NX_Address : NL_Integer, "", expr->pos);

	int L = lhs->num;
	int R = rhs->num;

	if (strcmp (name, "+") == 0)
	{
		result->num = L + R;
	}
	else if (strcmp (name, "-") == 0)
	{
		result->num = L - R;
	}
	else if (strcmp (name, "*") == 0)
	{
		result->num = L * R;
	}
	else if (strcmp (name, "/") == 0)
	{
		if (R == 0)
			goto division_by_zero;

		result->num = L / R;
	}
	else if (strcmp (name, "%") == 0)
	{
		if (R == 0)
			goto division_by_zero;

		result->num = L % R;
	}
	else if (strcmp (name, "|") == 0)
	{
		result->num = L | R;
	}
	else if (strcmp (name, "&") == 0)
	{
		result->num = L & R;
	}
	else if (strcmp (name, "^") == 0)
	{
		result->num = L ^ R;
	}
	else if (strcmp (name, "<<") == 0)
	{
		if (R < 0)
			goto bad_shift_width;

		result->num = L << R;
	}
	else if (strcmp (name, ">>") == 0)
	{
		if (R < 0)
			goto bad_shift_width;

		// unsigned ("logical") right shift
		result->num = (int32_t) ((uint32_t)L >> R);
	}
	else if (strcmp (name, ">>>") == 0)
	{
		if (R < 0)
			goto bad_shift_width;

		// signed ("arithmetic") right shift
		result->num = ((int32_t)L >> R);
	}
	else
	{
		Fatal_Error ("BUG: unimplemented operator '%s'\n", name);
	}

	return result;

	division_by_zero:
	{
		PostError ("division by zero with '%s' operator", name);
		return P_FAIL;
	}

	bad_shift_width:
	{
		PostError ("bad shift width with '%s' operator", name);
		return P_FAIL;
	}
}

//----------------------------------------------------------------------

struct Node * Eval_Label (struct Node * T, enum EvalContext ctx)
{
	if (ctx == ECTX_Constant)
	{
		PostError ("cannot use labels in constants");
		return P_FAIL;
	}

	if (ctx == ECTX_Assemble)
	{
		T->kind = NX_Address;
		T->num  = AddressForLabel (T->data.lab);
		return T;
	}

	return P_UNKNOWN;
}

struct Node * Eval_MakeLabel (struct Node * T, struct LabelDef * lab, enum EvalContext ctx)
{
	if (T->kind != NI_Label)
	{
		// convert existing node (NL_Word) to a NI_Label
		T->kind     = NI_Label;
		T->data.lab = lab;
	}

	return Eval_Label (T, ctx);
}

struct Node * Eval_MakeGlobal (struct Node * T, struct GlobalVar * glob, enum EvalContext ctx)
{
	if (ctx == ECTX_Constant)
	{
		PostError ("cannot use global variables in constants");
		return P_FAIL;
	}

	// convert existing node to a NX_Global
	T->kind = NX_Global;
	T->num  = glob->id;

	return T;
}

struct Node * Eval_Word (struct Node * T, enum EvalContext ctx, struct FunctionDef * cur_func)
{
	const char * name = Node_Str (T);

	if (name[0] == '.')
	{
		if (cur_func == NULL)
		{
			PostError ("private label '%s' used outside of a function", name);
			return P_FAIL;
		}

		struct LabelDef * lab = F_FindPrivateLabel (cur_func, name);
		if (lab == NULL)
		{
			PostError ("private label '%s' not found", name);
			return P_FAIL;
		}

		return Eval_MakeLabel (T, lab, ctx);
	}

	// check for local variables
	if (ctx == ECTX_Resolve && cur_func != NULL)
	{
		const char * name = Node_Str (T);
		struct LocalVar * var = F_FindLocalVar (cur_func, name);

		if (var != NULL)
		{
			T->kind = NX_Local;
			T->num  = var->offset;
			return T;
		}
	}

	struct Definition * def = SYM_Find (all_symbols, name);

	if (def == NULL)
	{
		PostError ("symbol '%s' was never defined", name);
		return P_FAIL;
	}

	if (def->kind == DEF_Label)
		return Eval_MakeLabel (T, def->u.lab, ctx);

	if (def->kind == DEF_Global)
		return Eval_MakeGlobal (T, def->u.glob, ctx);

	Assert (def->kind == DEF_Constant);

	struct ConstantDef * con = def->u.con;

	if (ctx == ECTX_Constant)
	{
		if (con->value == NULL)
			return P_UNKNOWN;

		if (con->value == P_FAIL)
			return P_FAIL;
	}

	// an earlier pass should have given the constant a value
	Assert (con->value != NULL);
	Assert (con->value != P_FAIL);

	return Node_Copy (con->value);
}

struct Node * Eval_Address (struct Node * T, enum EvalContext ctx, struct FunctionDef * cur_func)
{
	if (ctx == ECTX_Resolve)
		return P_UNKNOWN;

	struct Node * sub_expr = T->children;

	// just a simple (already evaluated) address?
	if (sub_expr == NULL)
		return T;

	struct Node * V;

	if (sub_expr->kind == NI_Label)
	{
		V = Node_New (NX_Address, "", T->pos);
		V->num = AddressForLabel (sub_expr->data.lab);
		return V;
	}
	else
	{
		struct Node * V = Evaluate (sub_expr, ctx, cur_func);
		if (V == P_FAIL)
			return P_FAIL;

		Assert (V->kind == NX_Address);
		return V;
	}
}

//----------------------------------------------------------------------

// evaluate an expression (which may just be a simple value) and
// return the result.  the result is a simple value, like NL_Integer.
//
// when `ctx` is ECTX_Constant, the result can be P_UNKNOWN which
// means it contains the name of a yet-to-be-evaluated constant.
//
// when `ctx` is ECTX_Resolve, a P_UNKNOWN result means the expression
// contains a label (its value won't be settled until resolve pass is
// finished).

struct Node * Evaluate (struct Node * expr, enum EvalContext ctx, struct FunctionDef * cur_func)
{
	SetErrorPos (expr->pos);

	switch (expr->kind)
	{
		case NL_Integer:
		case NL_Float:
		case NL_FltSpec:
		case NL_Char:
		case NL_String:
			return expr;

		case NL_Word:
			return Eval_Word (expr, ctx, cur_func);

		case NI_Label:
			return Eval_Label (expr, ctx);

		case NX_Unary:
			return Eval_Unary (expr, ctx, cur_func);

		case NX_Binary:
			return Eval_Binary (expr, ctx, cur_func);

		case NX_Address:
			return Eval_Address (expr, ctx, cur_func);

		default:
			Fatal_Error ("BUG: unknown node in expression\n");
			return P_FAIL;
	}
}

