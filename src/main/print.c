/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995-1998  Robert Gentleman and Ross Ihaka.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  print.default()  ->	 do_printdefault & its sub-functions.
 *			 do_printmatrix, do_sink, do_invisible
 *
 *  do_printdefault
 *	-> PrintDefaults
 *	-> CustomPrintValue
 *	    -> PrintValueRec
 *		-> __ITSELF__  (recursion)
 *		-> PrintGenericVector	-> PrintValueRec  (recursion)
 *		-> PrintList		-> PrintValueRec  (recursion)
 *		-> printAttributes	-> PrintValueRec  (recursion)
 *		-> PrintExpression
 *		-> printVector		>>>>> ./printvector.c
 *		-> printNamedVector	>>>>> ./printvector.c
 *		-> printMatrix		>>>>> ./printarray.c
 *		-> printArray		>>>>> ./printarray.c
 *
 *  do_printmatrix
 *	-> PrintDefaults
 *	-> printMatrix			>>>>> ./printarray.c
 *
 *
 *  See ./printutils.c	 for general remarks on Printing
 *			 and the Encode.. utils.
 *
 *  Also ./printvector.c,  ./printarray.c
 */

#ifdef HAVE_CONFIG_H
#include <Rconfig.h>
#endif

#include "Defn.h"
#include "Print.h"
#include "Fileio.h"
#include "S.h"

/* Global print parameter struct: */
R_print_par_t R_print;

static void printAttributes(SEXP, SEXP);

#define TAGBUFLEN 256
static char tagbuf[TAGBUFLEN + 5];


void PrintDefaults(SEXP rho)
{
    R_print.na_string = NA_STRING;
    R_print.na_width = strlen(CHAR(R_print.na_string));
    R_print.quote = 1;
    R_print.right = 0;
    R_print.digits = GetOptionDigits(rho);
    R_print.gap = 1;
    R_print.width = GetOptionWidth(rho);
}

SEXP do_sink(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    FILE *fp;
    SEXP file;
    int  append;

    file = CAR(args);
    append = asLogical(CADR(args));
    if (append == NA_LOGICAL)
        errorcall(call, "invalid append specification");

    if(isNull(file)) {
	if(R_Sinkfile) fclose(R_Sinkfile);
	R_Sinkfile = NULL;
	R_Outputfile = R_Consolefile;
    } else {
	if (!isString(file) || length(file) != 1)
	    errorcall(call, "invalid file name");
	if (append)
	    fp = R_fopen(R_ExpandFileName(CHAR(STRING(file)[0])), "a");
	else
	    fp = R_fopen(R_ExpandFileName(CHAR(STRING(file)[0])), "w");
	if (!fp)
	    errorcall(call, "unable to open file");
	R_Sinkfile = fp;
	R_Outputfile = fp;
    }
    return R_NilValue;
}

SEXP do_invisible(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    switch (length(args)) {
    case 0:
	return R_NilValue;
    case 1:
	return CAR(args);
    default:
	checkArity(op, args);
	return call;/* never used, just for -Wall */
    }
}

SEXP do_visibleflag(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    return ScalarLogical(R_Visible);
}

SEXP do_printmatrix(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    int quote;
    SEXP a, x, rowlab, collab;
    char *rowname = NULL, *colname = NULL;
#ifdef OLD
    SEXP oldnames;
#endif
    checkArity(op,args);
    PrintDefaults(rho);
    a = args;
    x = CAR(a); a = CDR(a);
    rowlab = CAR(a); a = CDR(a);
    collab = CAR(a); a = CDR(a);

    quote = asInteger(CAR(a)); a = CDR(a);
    R_print.right = asInteger(CAR(a));

#ifdef OLD
    PROTECT(oldnames = getAttrib(x, R_DimNamesSymbol));
    /* fix up the dimnames */
    if (length(rowlab) || length(collab) ||
	rowlab == R_NilValue || collab == R_NilValue) {
	a = oldnames;
	if(a == R_NilValue)
	    a = allocList(2);
	if(length(rowlab) || rowlab==R_NilValue)
	    CAR(a) = rowlab;
	if(length(collab) || collab==R_NilValue)
	    CADR(a) = collab;
	PROTECT(a);
	setAttrib(x, R_DimNamesSymbol, a);
	UNPROTECT(1);
    }
#else
    if (length(rowlab) == 0) rowlab = R_NilValue;
    if (length(collab) == 0) collab = R_NilValue;
#endif
    if (!isNull(rowlab) && !isString(rowlab))
	errorcall(call, "invalid row labels");
    if (!isNull(collab) && !isString(collab))
	errorcall(call, "invalid column labels");

    printMatrix(x, 0, getAttrib(x, R_DimSymbol), quote, R_print.right, rowlab, collab, rowname, colname);
#ifdef OLD
    setAttrib(x, R_DimNamesSymbol, oldnames);
    UNPROTECT(1);
#endif
    return x;
}/* do_printmatrix */


/* .Internal(print.default(x, digits, quote, na.print, print.gap)) */
SEXP do_printdefault(SEXP call, SEXP op, SEXP args, SEXP rho){

/* FIXME:
 * Should now also dispatch to e.g., print.matrix(..)
 * The 'digits' must be "stored" here, since print.matrix
 * (aka prmatrix) does NOT accept a digits argument ...
 */

    SEXP x, naprint;
    checkArity(op, args);
    PrintDefaults(rho);

    x = CAR(args); args = CDR(args);

    if(!isNull(CAR(args))) {
	R_print.digits = asInteger(CAR(args));
	if (R_print.digits == NA_INTEGER ||
	    R_print.digits < 1 ||
	    R_print.digits > 22)
		errorcall(call, "invalid digits parameter");
    }
    args = CDR(args);

    R_print.quote = asLogical(CAR(args));
    if(R_print.quote == NA_LOGICAL)
	errorcall(call, "invalid quote parameter");
    args = CDR(args);

    naprint = CAR(args);
    if(!isNull(naprint))  {
	if(!isString(naprint) || LENGTH(naprint) < 1)
	    errorcall(call, "invalid na.print specification");
	R_print.na_string = STRING(naprint)[0];
	R_print.na_width = strlen(CHAR(R_print.na_string));
    }
    args = CDR(args);

    if(!isNull(CAR(args))) {
	R_print.gap = asInteger(CAR(args));
	if (R_print.gap == NA_INTEGER || R_print.gap < 1 || R_print.gap > 10)
	    errorcall(call, "invalid gap parameter");
    }
    args = CDR(args);

    R_print.right = asLogical(CAR(args));
    if(R_print.right == NA_LOGICAL)
	errorcall(call, "invalid right parameter");
    args = CDR(args);

    CustomPrintValue(x, rho);
    return x;
}/* do_printdefault */


/* FIXME : We need a general mechanism for "rendering" symbols. */
/* It should make sure that it quotes when there are special */
/* characters and also take care of ansi escapes properly. */

static void PrintGenericVector(SEXP s, SEXP env)
{
    int i, taglen, ns;
    SEXP dims, t, names, newcall, tmp;
    char *pbuf, *ptag, *rn, *cn;

    ns = length(s);
    if((dims = getAttrib(s, R_DimSymbol)) != R_NilValue && length(dims) > 1) {
	PROTECT(dims);
	PROTECT(t = allocArray(STRSXP, dims));
	for (i = 0 ; i < ns ; i++) {
	    switch(TYPEOF(PROTECT(tmp = VECTOR(s)[i]))) {
	    case NILSXP:
		pbuf = Rsprintf("NULL");
		break;
	    case LGLSXP:
		pbuf = Rsprintf("Logical,%d", LENGTH(tmp));
		break;
	    case INTSXP:
	    case REALSXP:
		pbuf = Rsprintf("Numeric,%d", LENGTH(tmp));
		break;
	    case CPLXSXP:
		pbuf = Rsprintf("Complex,%d", LENGTH(tmp));
		break;
	    case STRSXP:
		pbuf = Rsprintf("Character,%d", LENGTH(tmp));
		break;
	    case LISTSXP:
	    case VECSXP:
		pbuf = Rsprintf("List,%d", length(tmp));
		break;
	    case LANGSXP:
		pbuf = Rsprintf("Expression");
		break;
	    default:
		pbuf = Rsprintf("?");
		break;
	    }
	    UNPROTECT(1); /* tmp */
	    STRING(t)[i] = mkChar(pbuf);
	}
	if (LENGTH(dims) == 2) {
	    SEXP rl, cl;
	    GetMatrixDimnames(s, &rl, &cl, &rn, &cn);
	    printMatrix(t, 0, dims, R_print.quote, R_print.right, rl, cl,
			rn, cn);
	}
	else {
	    names = GetArrayDimnames(s);
	    printArray(t, dims, 0, names);
	}
	UNPROTECT(2);
    }
    else {
	names = getAttrib(s, R_NamesSymbol);
	taglen = strlen(tagbuf);
	ptag = tagbuf + taglen;
	PROTECT(newcall = allocList(2));
	CAR(newcall) = install("print");
	TYPEOF(newcall) = LANGSXP;

	if(ns > 0) {
	for (i = 0 ; i < ns ; i++) {
	    if (i > 0) Rprintf("\n");
	    if (names != R_NilValue &&
		STRING(names)[i] != R_NilValue &&
		*CHAR(STRING(names)[i]) != '\0') {
		if (taglen + strlen(CHAR(STRING(names)[i])) > TAGBUFLEN)
		    sprintf(ptag, "$...");
		else {
		    if( isValidName(CHAR(STRING(names)[i])) )
			sprintf(ptag, "$%s", CHAR(STRING(names)[i]));
		    else
			sprintf(ptag, "$\"%s\"", CHAR(STRING(names)[i]));
		}
	    }
	    else {
		if (taglen + IndexWidth(i) > TAGBUFLEN)
		    sprintf(ptag, "$...");
		else
		    sprintf(ptag, "[[%d]]", i+1);
	    }
	    Rprintf("%s\n", tagbuf);
	    if(isObject(VECTOR(s)[i])) {
		CADR(newcall) = VECTOR(s)[i];
		eval(newcall, env);
	    }
	    else PrintValueRec(VECTOR(s)[i], env);
	    *ptag = '\0';
	}
	Rprintf("\n");
	}
	else Rprintf("list()\n");
	UNPROTECT(1);
    }
}


static void printList(SEXP s, SEXP env)
{
    int i, taglen;
    SEXP dims, dimnames, t, newcall;
    char *pbuf, *ptag, *rn, *cn;

    if ((dims = getAttrib(s, R_DimSymbol)) != R_NilValue && length(dims) > 1) {
	PROTECT(dims);
	PROTECT(t = allocArray(STRSXP, dims));
	i = 0;
	while(s != R_NilValue) {
	    switch(TYPEOF(CAR(s))) {

	    case NILSXP:
		pbuf = Rsprintf("NULL");
		break;

	    case LGLSXP:
		pbuf = Rsprintf("Logical,%d", LENGTH(CAR(s)));
		break;

	    case INTSXP:
	    case REALSXP:
		pbuf = Rsprintf("Numeric,%d", LENGTH(CAR(s)));
		break;

	    case CPLXSXP:
		pbuf = Rsprintf("Complex,%d", LENGTH(CAR(s)));
		break;

	    case STRSXP:
		pbuf = Rsprintf("Character,%d", LENGTH(CAR(s)));
		break;

	    case LISTSXP:
		pbuf = Rsprintf("List,%d", length(CAR(s)));
		break;

	    case LANGSXP:
		pbuf = Rsprintf("Expression");
		break;

	    default:
		pbuf = Rsprintf("?");
		break;
	    }
	    STRING(t)[i++] = mkChar(pbuf);
	    s = CDR(s);
	}
	if (LENGTH(dims) == 2) {
	    SEXP rl, cl;
	    GetMatrixDimnames(s, &rl, &cl, &rn, &cn);
	    printMatrix(t, 0, dims, R_print.quote, R_print.right, rl, cl,
			rn, cn);
	}
	else {
	    dimnames = getAttrib(s, R_DimNamesSymbol);
	    printArray(t, dims, 0, dimnames);
	}
	UNPROTECT(2);
    }
    else {
	i = 1;
	taglen = strlen(tagbuf);
	ptag = tagbuf + taglen;
	PROTECT(newcall = allocList(2));
	CAR(newcall) = install("print");
	TYPEOF(newcall) = LANGSXP;
	while (TYPEOF(s) == LISTSXP) {
	    if (i > 1) Rprintf("\n");
	    if (TAG(s) != R_NilValue && isSymbol(TAG(s))) {
		if (taglen + strlen(CHAR(PRINTNAME(TAG(s)))) > TAGBUFLEN)
		    sprintf(ptag, "$...");
		else {
		    if( isValidName(CHAR(PRINTNAME(TAG(s)))) )
			sprintf(ptag, "$%s", CHAR(PRINTNAME(TAG(s))));
		    else
			sprintf(ptag, "$\"%s\"", CHAR(PRINTNAME(TAG(s))));
		}
	    }
	    else {
		if (taglen + IndexWidth(i) > TAGBUFLEN)
		    sprintf(ptag, "$...");
		else
		    sprintf(ptag, "[[%d]]", i);
	    }
	    Rprintf("%s\n", tagbuf);
	    if(isObject(CAR(s))) {
		CADR(newcall) = CAR(s);
		eval(newcall, env);
	    }
	    else PrintValueRec(CAR(s),env);
	    *ptag = '\0';
	    s = CDR(s);
	    i += 1;
	}
	if (s != R_NilValue) {
	    Rprintf("\n. \n\n");
	    PrintValueRec(s,env);
	}
	Rprintf("\n");
	UNPROTECT(1);
    }
    printAttributes(s,env);
}

static void PrintExpression(SEXP s)
{
    SEXP u;
    int i, n;

    u = deparse1(s, 0);
    n = LENGTH(u);
    for (i=0; i<n ; i++)
	Rprintf("%s\n", CHAR(STRING(u)[i]));
}


/* PrintValueRec -- recursively print an SEXP

 * This is the "dispatching" function for  print.default()
 */
void PrintValueRec(SEXP s,SEXP env)
{
    int i;
    SEXP t;

    switch (TYPEOF(s)) {
    case NILSXP:
	Rprintf("NULL\n");
	break;
    case SYMSXP:
	Rprintf("%s\n", CHAR(PRINTNAME(s)));
	break;
    case SPECIALSXP:
    case BUILTINSXP:
	Rprintf(".Primitive(\"%s\")\n", PRIMNAME(s));
	break;
    case CHARSXP:
	Rprintf("\"%s\"\n", EncodeString(CHAR(s),0,'"', adj_left));
	break;
    case EXPRSXP:
	PrintExpression(s);
	break;
    case CLOSXP:
    case LANGSXP:
	t = getAttrib(s, R_SourceSymbol);
	if (isNull(t))
	    t = deparse1(s, 0);
	for (i = 0; i < LENGTH(t); i++)
	    Rprintf("%s\n", CHAR(STRING(t)[i]));
	if (TYPEOF(s) == CLOSXP) t = CLOENV(s);
	else t = R_GlobalEnv;
	if (t != R_GlobalEnv)
	    Rprintf("<environment: %p>\n", t);
	break;
    case ENVSXP:
	if (s == R_GlobalEnv) Rprintf("<environment: R_GlobalEnv>\n");
	else Rprintf("<environment: %p>\n", s);
	break;
    case PROMSXP:
	Rprintf("<promise: %p>\n", s);
	break;
    case DOTSXP:
	Rprintf("<...>\n");
	break;
    case VECSXP:
	PrintGenericVector(s, env);
	break;
    case LISTSXP:
	printList(s,env);
	break;
    case LGLSXP:
    case INTSXP:
    case REALSXP:
    case STRSXP:
    case CPLXSXP:
	PROTECT(t = getAttrib(s, R_DimSymbol));
	if (TYPEOF(t) == INTSXP) {
	    if (LENGTH(t) == 1) {
		PROTECT(t = getAttrib(s, R_DimNamesSymbol));
		if (t != R_NilValue && VECTOR(t)[0] != R_NilValue) {
		    SEXP nn = getAttrib(t, R_NamesSymbol);
		    char *title = NULL;

		    if (!isNull(nn))
		        title = CHAR(STRING(nn)[0]);

		    printNamedVector(s, VECTOR(t)[0], R_print.quote, title);
		}
		else
		    printVector(s, 1, R_print.quote);
		UNPROTECT(1);
	    }
	    else if (LENGTH(t) == 2) {
		SEXP rl, cl;
		char *rn, *cn;
		GetMatrixDimnames(s, &rl, &cl, &rn, &cn);
		printMatrix(s, 0, t, R_print.quote, R_print.right, rl, cl,
			    rn, cn);
	    }
	    else {
		SEXP dimnames;
		dimnames = GetArrayDimnames(s);
		printArray(s, t, R_print.quote, dimnames);
	    }
	}
	else {
	    UNPROTECT(1);
	    PROTECT(t = getAttrib(s, R_NamesSymbol));
	    if (t != R_NilValue)
		printNamedVector(s, t, R_print.quote, NULL);
	    else
		printVector(s, 1, R_print.quote);
	}
	UNPROTECT(1);
	break;
    default:
	UNIMPLEMENTED("PrintValueRec");
    }
    printAttributes(s,env);
}

static void printAttributes(SEXP s,SEXP env)
{
    SEXP a;
    SEXP R_LevelsSymbol;
    char *ptag;
    int i;

    a = ATTRIB(s);
    if (a != R_NilValue) {
	R_LevelsSymbol = install("levels");
	ptag = tagbuf + strlen(tagbuf);
	i = 1;
	while (a != R_NilValue) {
	    if(isArray(s) || isList(s)) {
		if(TAG(a) == R_DimSymbol ||
		   TAG(a) == R_DimNamesSymbol)
		    goto nextattr;
	    }
	    if(isFactor(s)) {
		if(TAG(a) == R_LevelsSymbol)
		    goto nextattr;
		if(TAG(a) == R_ClassSymbol)
		    goto nextattr;
	    }
	    if(isFrame(s)) {
		if(TAG(a) == R_RowNamesSymbol)
		    goto nextattr;
	    }
	    if(!isArray(s)) {
		if (TAG(a) == R_NamesSymbol)
		    goto nextattr;
	    }
	    if(TAG(a) == R_CommentSymbol || TAG(a) == R_SourceSymbol)
		goto nextattr;
	    if (i > 1)
		Rprintf("\n");
	    sprintf(ptag, "attr(,\"%s\")",
		    EncodeString(CHAR(PRINTNAME(TAG(a))),0,0, adj_left));
	    Rprintf("%s\n", tagbuf);
	    PrintValueRec(CAR(a),env);
	nextattr:
	    *ptag = '\0';
	    a = CDR(a);
	}
    }
}


/* Print an S-expression using (possibly) local options */

void PrintValueEnv(SEXP s, SEXP env)
{
    SEXP call;

    PrintDefaults(env);
    tagbuf[0] = '\0';
    PROTECT(s);
    if(isObject(s)) {
	PROTECT(call = lang2(install("print"), s));
	eval(call, env);
	UNPROTECT(1);
    }
    else {
	PrintValueRec(s,env);
    }
    UNPROTECT(1);
}


/* Print an S-expression using global options */

void PrintValue(SEXP s)
{
    PrintValueEnv(s, R_NilValue);
}


void CustomPrintValue(SEXP s, SEXP env)
{
    tagbuf[0] = '\0';
    PrintValueRec(s, env);
}


/* dblepr and intpr are mostly for S compatibility */
/* (as mentioned in V&R) */

int F77_SYMBOL(dblepr) (char *label, int *nchar, double *data, int *ndata)
{
    int k;
    for (k = 0; k < *nchar; k++){
	Rprintf("%c", label[k]);
    }
    Rprintf("\n");
    printRealVector(data, *ndata, 1);
    return(0);
}

int F77_SYMBOL(intpr) (char *label, int *nchar, int *data, int *ndata)
{
    int k;
    for (k = 0; k < *nchar; k++){
	Rprintf("%c", label[k]);
    }
    Rprintf("\n");
    printIntegerVector(data, *ndata, 1);
    return(0);
}

int F77_SYMBOL(realpr) (char *label, int *nchar, float *data, int *ndata)
{
    int k;
    double *ddata;
    
    ddata = malloc((*ndata)*sizeof(double));
    if(!ddata) error("memory allocation error in realpr");
    for (k = 0; k < *ndata; k++) ddata[k] = (double) data[k];
    for (k = 0; k < *nchar; k++) {
	Rprintf("%c", label[k]);
    }
    Rprintf("\n");
    printRealVector(ddata, *ndata, 1);
    free(ddata);
    return(0);
}
