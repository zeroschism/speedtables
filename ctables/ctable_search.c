/*
 * Ctable search routines
 *
 * $Id$
 *
 */

#include "ctable.h"

/*
 * ctable_ParseFieldList - given a Tcl list object and a pointer to an array
 * of integer field numbers and a pointer to an integer for field counts,
 * install the field count into the field count and allocate an array of 
 * integers for the corresponding field indexes and fill that array with the 
 * field numbers corresponding to the field names in the list.
 *
 * It is up to the caller to free the memory pointed to through the
 * fieldList argument.
 *
 * return TCL_OK if all went according to plan, else TCL_ERROR.
 *
 */
int
ctable_ParseFieldList (Tcl_Interp *interp, Tcl_Obj *fieldListObj, CONST char **fieldNames, int **fieldListPtr, int *fieldCountPtr) {
    int             nFields;
    Tcl_Obj       **fieldsObjv;
    int             i;
    int            *fieldList;

    // the fields they want us to retrieve
    if (Tcl_ListObjGetElements (interp, fieldListObj, &nFields, &fieldsObjv) == TCL_ERROR) {
      return TCL_ERROR;
    }

    *fieldCountPtr = nFields;
    *fieldListPtr = fieldList = (int *)ckalloc (sizeof (int) * nFields);

    for (i = 0; i < nFields; i++) {
	if (Tcl_GetIndexFromObj (interp, fieldsObjv[i], fieldNames, "field", TCL_EXACT, &fieldList[i]) != TCL_OK) {
	    ckfree ((void *)fieldList);
	    *fieldListPtr = NULL;
	    return TCL_ERROR;
	  }
    }
    return TCL_OK;
}

/*
 * ctable_ParseSortFieldList - given a Tcl list object, and a pointer to a
 * ctable sort structure, store the number of fields in the list in the
 * sort structure's field count.  allocate an array of integers for the
 * field numbers and directions and store them into the sort structure passed.
 *
 * Strip the prepending dash of each field, if present, and do the lookup
 * and store the field number in the corresponding field number array.
 *
 * If the dash was present set the corresponding direction in the direction
 * array to 0 else set it to 1.
 *
 * It is up to the caller to free the memory pointed to through the
 * fieldList argument.
 *
 * return TCL_OK if all went according to plan, else TCL_ERROR.
 *
 */
int
ctable_ParseSortFieldList (Tcl_Interp *interp, Tcl_Obj *fieldListObj, CONST char **fieldNames, struct ctableSortStruct *sort) {
    int             nFields;
    Tcl_Obj       **fieldsObjv;
    Tcl_Obj        *fieldNameObj;
    int             i;
    char           *fieldName;

    // the fields they want us to retrieve
    if (Tcl_ListObjGetElements (interp, fieldListObj, &nFields, &fieldsObjv) == TCL_ERROR) {
      return TCL_ERROR;
    }

    sort->nFields = nFields;
    sort->fields =  (int *)ckalloc (sizeof (int) * nFields);
    sort->directions =  (int *)ckalloc (sizeof (int) * nFields);

    for (i = 0; i < nFields; i++) {
        fieldName = Tcl_GetString (fieldsObjv[i]);
	if (fieldName[0] == '-') {
	    sort->directions[i] = -1;
	    fieldName++;
	    fieldNameObj = Tcl_NewStringObj (fieldName, -1);
	} else {
	    fieldNameObj = fieldsObjv[i];
	    sort->directions[i] = 1;
	}

	if (Tcl_GetIndexFromObj (interp, fieldNameObj, fieldNames, "field", TCL_EXACT, &sort->fields[i]) != TCL_OK) {
	    ckfree ((void *)sort->fields);
	    ckfree ((void *)sort->directions);
	    sort->fields = NULL;
	    sort->directions = NULL;
	    return TCL_ERROR;
	  }
    }
    return TCL_OK;
}

static int
ctable_ParseSearch (Tcl_Interp *interp, Tcl_Obj *componentListObj, CONST char **fieldNames, struct ctableSearchStruct *search) {
    Tcl_Obj    **componentList;
    int          componentIdx;
    int          componentListCount;

    Tcl_Obj    **termList;
    int          termListCount;

    int          field;

    struct ctableSearchComponentStruct  *components;
    struct ctableSearchComponentStruct  *component;
    
    // these terms must line up with the CTABLE_COMP_* defines
    static CONST char *searchTerms[] = {"false", "true", "null", "notnull", "<", "<=", "=", "!=", ">=", ">", (char *)NULL};

    if (Tcl_ListObjGetElements (interp, componentListObj, &componentListCount, &componentList) == TCL_ERROR) {
        return TCL_ERROR;
    }

    if (componentListCount == 0) {
        search->components = NULL;
	return TCL_OK;
    }

    search->nComponents = componentListCount;

    components = (struct ctableSearchComponentStruct *)ckalloc (componentListCount * sizeof (struct ctableSearchComponentStruct));

    search->components = components;

    for (componentIdx = 0; componentIdx < componentListCount; componentIdx++) {
        int term;

	if (Tcl_ListObjGetElements (interp, componentList[componentIdx], &termListCount, &termList) == TCL_ERROR) {
	  err:
	    ckfree ((char *)components);
	    return TCL_ERROR;
	}

	if (termListCount < 2 || termListCount > 3) {
	    // would be cool to support regexps here too
	    Tcl_WrongNumArgs (interp, 0, termList, "term field ?value?");
	    goto err;
	}

	if (Tcl_GetIndexFromObj (interp, termList[0], searchTerms, "term", TCL_EXACT, &term) != TCL_OK) {
	    goto err;
	}

	if (Tcl_GetIndexFromObj (interp, termList[1], fieldNames, "field", TCL_EXACT, &field) != TCL_OK) {
	    goto err;
	}

	component = &components[componentIdx];

	component->comparisonType = term;
	component->fieldID = field;

	if (term == CTABLE_COMP_FALSE || term == CTABLE_COMP_TRUE || term == CTABLE_COMP_NULL || term == CTABLE_COMP_NOTNULL) {
	    component->comparedToObject = NULL;
	    if (termListCount != 2) {
		Tcl_AppendResult (interp, "false, true, null and notnull search expressions must have only two fields", (char *) NULL);
		return TCL_ERROR;
	    }
	}  else {
	    if (termListCount != 3) {
		Tcl_AppendResult (interp, "term \"", Tcl_GetString (termList[0]), "\" require 3 arguments (term, field, value)", (char *) NULL);
		return TCL_ERROR;
	    }
	    component->comparedToObject = termList[2];
	}
    }

    // it worked, leave the components allocated
    return TCL_OK;
}

static int
ctable_SearchAction (Tcl_Interp *interp, struct ctableTable *ctable, struct ctableSearchStruct *search, Tcl_HashTable *keyTablePtr, Tcl_HashEntry *hashEntry) {
    char           *key;
    void           *p;
    int             i;

    key = Tcl_GetHashKey (keyTablePtr, hashEntry);
    p = Tcl_GetHashValue (hashEntry);

    if (search->writingTabsep) {
	Tcl_DString     dString;

	Tcl_DStringInit (&dString);

        if (search->nRetrieveFields < 0) {
	    (*ctable->creatorTable->dstring_append_get_tabsep) (key, p, ctable->creatorTable->fieldList, ctable->creatorTable->nFields, &dString, search->noKeys);
	} else {
	    (*ctable->creatorTable->dstring_append_get_tabsep) (key, p, search->retrieveFields, search->nRetrieveFields, &dString, search->noKeys);
	}

	if (Tcl_WriteChars (search->tabsepChannel, Tcl_DStringValue (&dString), Tcl_DStringLength (&dString)) < 0) {
	    return TCL_ERROR;
	}

	Tcl_DStringFree (&dString);
	return TCL_OK;
    }

    if (search->codeBody != NULL) {
	Tcl_Obj *listObj = Tcl_NewObj();
	int      evalResult;

	if (search->useGet) {
	    if (search->nRetrieveFields < 0) {
		listObj = (*ctable->creatorTable->gen_list) (interp, p);
	    } else {
	       int i;

	       for (i = 0; i < search->nRetrieveFields; i++) {
		   ctable->creatorTable->lappend_field (interp, listObj, p, ctable->creatorTable->fieldList[i]);
	       }
	    }
	} else if (search->useArrayGet) {
	    if (search->nRetrieveFields < 0) {
	       int i;

	       for (i = 0; i < ctable->creatorTable->nFields; i++) {
		   ctable->creatorTable->lappend_nonnull_field_and_name (interp, listObj, p, i);
	       }
	    } else {
	       int i;

	       for (i = 0; i < search->nRetrieveFields; i++) {
		   ctable->creatorTable->lappend_nonnull_field_and_name (interp, listObj, p, search->retrieveFields[i]);
	       }
	    }
	} else if (search->useArrayGetWithNulls) {
	    if (search->nRetrieveFields < 0) {
		listObj = (*ctable->creatorTable->gen_keyvalue_list) (interp, p);
	    } else {
		for (i = 0; i < search->nRetrieveFields; i++) {
		    ctable->creatorTable->lappend_field_and_name (interp, listObj, p, search->retrieveFields[i]);
		}
	    }
	} else {
	    panic ("code path shuld have matched useArrayGet or useArrayGetWithNulls");
	}

	// if the key var is defined, set the key into it
	if (search->keyVarNameObj != NULL) {
	    if (Tcl_ObjSetVar2 (interp, search->keyVarNameObj, (Tcl_Obj *)NULL, Tcl_NewStringObj (key, -1), TCL_LEAVE_ERR_MSG) == (Tcl_Obj *) NULL) {
		return TCL_ERROR;
	    }
	}

	// set the returned list into the value var
	if (Tcl_ObjSetVar2 (interp, search->varNameObj, (Tcl_Obj *)NULL, listObj, TCL_LEAVE_ERR_MSG) == (Tcl_Obj *) NULL) {
	    return TCL_ERROR;
	}

	evalResult = Tcl_EvalObjEx (interp, search->codeBody, 0);
	switch (evalResult) {
	  case TCL_ERROR:
	    Tcl_AppendResult (interp, " while processing search code body", (char *) NULL);
	    return TCL_ERROR;

	  case TCL_OK:
	  case TCL_CONTINUE:
	  case TCL_BREAK:
	  case TCL_RETURN:
	    return evalResult;
	}
    }

    return TCL_OK;
}

//
// ctable_PerformSearch - 
//
//
//
static int
ctable_PerformSearch (Tcl_Interp *interp, struct ctableTable *ctable, struct ctableSearchStruct *search, int count) {
    Tcl_HashEntry   *hashEntry;
    Tcl_HashSearch   hashSearch;
    char            *key;

    int              compareResult;
    int              matchCount = 0;
    int              sortIndex;
    int              actionResult = TCL_OK;
    int              limit;


    Tcl_HashEntry **hashSortTable = NULL;

    Tcl_HashTable *keyTablePtr = ctable->keyTablePtr;

    if (count == 0) {
        return TCL_OK;
    }

    if ((search->sortControl.nFields > 0) && (!search->countOnly)) {
	hashSortTable = (Tcl_HashEntry **)ckalloc (sizeof (Tcl_HashEntry *) * count);
    }

    /* Build up a table of ptrs to hash entries of rows of the table.
     * Optional match pattern on the primary key means we may end up
     * with fewer than the total number.
    */
    for (hashEntry = Tcl_FirstHashEntry (keyTablePtr, &hashSearch); hashEntry != (Tcl_HashEntry *) NULL; hashEntry = Tcl_NextHashEntry (&hashSearch)) {

	key = Tcl_GetHashKey (keyTablePtr, hashEntry);

	if ((search->pattern != (char *) NULL) && (!Tcl_StringCaseMatch (key, search->pattern, 1))) continue;

	compareResult = (*ctable->creatorTable->search_compare) (interp, search, hashEntry);
	if (compareResult == TCL_CONTINUE) {
	    continue;
	}

	if (compareResult == TCL_ERROR) {
	    actionResult = TCL_ERROR;
	    goto clean_and_return;
	}

	/* It's a Match */

        /* Are we not sorting? */
	if (hashSortTable == NULL) {
	    /* if we haven't met the start point, blow it off */
	    if (++matchCount < search->offset) continue;

	    // if there is a limit and it's been exceeded, we're done
	    if ((search->limit != 0) && (matchCount >= search->limit)) {
		actionResult = TCL_OK;
		goto clean_and_return;
	    }

	    /* we want to take the match actions here --
	     * we're here when we aren't sorting
	     */
	     actionResult = ctable_SearchAction (interp, ctable, search, keyTablePtr, hashEntry);
	     if (actionResult == TCL_ERROR) {
		  goto clean_and_return;
	     }

	     if (actionResult == TCL_CONTINUE || actionResult == TCL_OK) {
		 continue;
	     }

	     if (actionResult == TCL_BREAK || actionResult == TCL_RETURN) {
		  actionResult = TCL_OK;
		  goto clean_and_return;
	     }
	// match, handle action or tabsep write
	} else {
	    /* We are sorting, grab it, we gotta sort before we can run
	     * against start and limit and stuff */
	    assert (matchCount < count);
	    // printf ("filling sort table %d -> hash entry %lx (%s)\n", matchCount, (long unsigned int)hashEntry, key);
	    hashSortTable[matchCount++] = hashEntry;
	}
    }

    // if we're not sorting, we're done -- we did 'em all on the fly
    if (hashSortTable == NULL) {
	actionResult = TCL_OK;
	goto clean_and_return;
    }

    qsort_r (hashSortTable, matchCount, sizeof (Tcl_HashEntry *), &search->sortControl, ctable->creatorTable->sort_compare);

    // it's sorted
    // now let's see what we've got within the offset and limit

    // if the offset's more than the matchCount, they got nuthin'
    if (search->offset > matchCount) {
	actionResult = TCL_OK;
	goto clean_and_return;
    }

    // determine start and limit
    limit = matchCount - search->offset;
    if ((search->limit > 0) && (search->limit < (matchCount - search->offset))) {
	limit = search->offset + search->limit;
    }

    // walk the result
    for (sortIndex = search->offset; sortIndex < limit; sortIndex++) {

	/* here is where we want to take the match actions
	 * when we are sorting
	 */
	 actionResult = ctable_SearchAction (interp, ctable, search, keyTablePtr, hashSortTable[sortIndex]);
	 if (actionResult == TCL_ERROR) {
	     goto clean_and_return;
	 }

	 if (actionResult == TCL_CONTINUE || actionResult == TCL_OK) {
	     continue;
	 }

	 if (actionResult == TCL_BREAK || actionResult == TCL_RETURN) {
	     actionResult = TCL_OK;
	     goto clean_and_return;
	 }
    }

  clean_and_return:
    if (hashSortTable != NULL) {
	ckfree ((void *)hashSortTable);
    }

    return actionResult;
}

static int
ctable_SetupSearch (Tcl_Interp *interp, Tcl_Obj *CONST objv[], int objc, struct ctableSearchStruct *search, CONST char **fieldNames) {
    int             i;
    int             searchTerm = 0;

    static CONST char *searchOptions[] = {"-array_get", "-array_get_with_nulls", "-code", "-compare", "-countOnly", "-fields", "-get", "-glob", "-key", "-limit", "-noKeys", "-offset", "-regexp", "-sort", "-write_tabsep", (char *)NULL};

    enum searchOptions {SEARCH_OPT_ARRAYGET_NAMEOBJ, SEARCH_OPT_ARRAYGETWITHNULLS_NAMEOBJ, SEARCH_OPT_CODE, SEARCH_OPT_COMPARE, SEARCH_OPT_COUNTONLY, SEARCH_OPT_FIELDS, SEARCH_OPT_GET_NAMEOBJ, SEARCH_OPT_GLOB, SEARCH_OPT_KEYVAR_NAMEOBJ, SEARCH_OPT_LIMIT, SEARCH_OPT_DONT_INCLUDE_KEY, SEARCH_OPT_OFFSET, SEARCH_OPT_REGEXP, SEARCH_OPT_SORT, SEARCH_OPT_WRITE_TABSEP};

    if (objc < 2) {
      wrong_args:
	Tcl_WrongNumArgs (interp, 2, objv, "?-sort {field1 {field2 desc}}? ?-fields fieldList? ?-glob pattern? ?-regexp pattern? ?-compare list? ?-noKeys 0|1? ?-contOnly 0|1? ?-offset offset? ?-limit limit? ?-code codeBody? ?-write_tabsep channel?");
	return TCL_ERROR;
    }

    // initialize search control structure
    search->nComponents = 0;
    search->components = NULL;
    search->countOnly = 0;
    search->countMax = 0;
    search->offset = 0;
    search->limit = 0;
    search->pattern = NULL;
    search->sortControl.fields = NULL;
    search->sortControl.directions = NULL;
    search->sortControl.nFields = 0;
    search->retrieveFields = NULL;
    search->nRetrieveFields = -1;   // -1 = all, 0 = none
    search->noKeys = 0;
    search->varNameObj = NULL;
    search->keyVarNameObj = NULL;
    search->useArrayGet = 0;
    search->useArrayGetWithNulls = 0;
    search->useGet = 0;
    search->codeBody = NULL;
    search->writingTabsep = 0;

    for (i = 2; i < objc; ) {
	if (Tcl_GetIndexFromObj (interp, objv[i++], searchOptions, "search option", TCL_EXACT, &searchTerm) != TCL_OK) {
	    return TCL_ERROR;
	}

	//  all the arguments require one parameter
	if (i >= objc) {
	    goto wrong_args;
	}

	switch (searchTerm) {
	  case SEARCH_OPT_SORT: {
	    // the fields they want us to sort on
	    if (ctable_ParseSortFieldList (interp, objv[i++], fieldNames, &search->sortControl) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing sort options", (char *) NULL);
	        return TCL_ERROR;
	    }
	    break;
	  }

	  case SEARCH_OPT_FIELDS: {
	    // the fields they want us to retrieve
	    if (ctable_ParseFieldList (interp, objv[i++], fieldNames, &search->retrieveFields, &search->nRetrieveFields) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing search fields", (char *) NULL);
	        return TCL_ERROR;
	    }

	    break;
	  }

	  case SEARCH_OPT_DONT_INCLUDE_KEY: {
	    if (Tcl_GetBooleanFromObj (interp, objv[i++], &search->noKeys) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing search noKeys", (char *) NULL);
	        return TCL_ERROR;
	    }
	    break;
	  }

	  case SEARCH_OPT_ARRAYGET_NAMEOBJ: {
	    search->varNameObj = objv[i++];
	    search->useArrayGet = 1;
	    break;
	  }

	  case SEARCH_OPT_ARRAYGETWITHNULLS_NAMEOBJ: {
	    search->varNameObj = objv[i++];
	    search->useArrayGetWithNulls = 1;
	    break;
	  }

	  case SEARCH_OPT_KEYVAR_NAMEOBJ: {
	    search->keyVarNameObj = objv[i++];
	    break;
          }

	  case SEARCH_OPT_GET_NAMEOBJ: {
	    search->varNameObj = objv[i++];
	    search->useGet = 1;
	    break;
          }

	  case SEARCH_OPT_GLOB: {
	    search->pattern = Tcl_GetString (objv[i++]);
	    break;
	  }

	  case SEARCH_OPT_REGEXP: {
	    Tcl_AppendResult (interp, "regexp not implemented yet", (char *) NULL);
	    return TCL_ERROR;
	  }

	  case SEARCH_OPT_COMPARE: {
	    if (ctable_ParseSearch (interp, objv[i++], fieldNames, search) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing search compare", (char *) NULL);
	        return TCL_ERROR;
	    }
	    break;
	  }

	  case SEARCH_OPT_COUNTONLY: {
	    if (Tcl_GetBooleanFromObj (interp, objv[i++], &search->countOnly) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing search countOnly", (char *) NULL);
	        return TCL_ERROR;
	    }
	    break;
	  }

	  case SEARCH_OPT_OFFSET: {
	    if (Tcl_GetIntFromObj (interp, objv[i++], &search->offset) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing search offset", (char *) NULL);
	        return TCL_ERROR;
	    }
	    break;
	  }

	  case SEARCH_OPT_LIMIT: {
	    if (Tcl_GetIntFromObj (interp, objv[i++], &search->limit) == TCL_ERROR) {
	        Tcl_AppendResult (interp, " while processing search limit", (char *) NULL);
	        return TCL_ERROR;
	    }
	    break;
	  }

	  case SEARCH_OPT_CODE: {
	      search->codeBody = objv[i++];
	      break;
	  }

	  case SEARCH_OPT_WRITE_TABSEP: {
	    int        mode;
	    char      *channelName;

	    channelName = Tcl_GetString (objv[i++]);
	    if ((search->tabsepChannel = Tcl_GetChannel (interp, channelName, &mode)) == NULL) {
	        Tcl_AppendResult (interp, " while processing write_tabsep channel", (char *) NULL);
	        return TCL_ERROR;
	    }

	    if (!(mode & TCL_WRITABLE)) {
		Tcl_AppendResult (interp, "channel \"", channelName, "\" not writable", (char *)NULL);
		return TCL_ERROR;
	    }

	    search->writingTabsep = 1;
	  }
	}
    }

    if (search->writingTabsep && (search->codeBody != NULL || search->keyVarNameObj != NULL || search->varNameObj != NULL)) {
	Tcl_AppendResult (interp, "can't use -code, -key, -array_get, -array_get_with_nulls  or -get along with -write_tabsep", (char *) NULL);
	return TCL_ERROR;
    }

    if (search->sortControl.nFields && search->countOnly) {
	Tcl_AppendResult (interp, "it's nuts to -sort something that's a -countOnly anyway", (char *) NULL);
	return TCL_ERROR;
    }

    if (search->useArrayGet + search->useArrayGetWithNulls + search->useGet > 1) {
	Tcl_AppendResult (interp, "-array_get, -array_get_with_nulls and -get options are mutually exclusive", (char *) NULL);
	return TCL_ERROR;
    }

    if (!search->useArrayGet && !search->useArrayGetWithNulls && !search->useGet && !search->writingTabsep) {
        Tcl_AppendResult (interp, "one of -array_get, -array_get_with_nulls, -get or -write_tabsep must be specified", (char *)NULL);
	return TCL_ERROR;
    }

    if (search->useArrayGet || search->useArrayGetWithNulls || search->useGet) {
        if (!search->codeBody) {
	    Tcl_AppendResult (interp, "-code must be set if -array_get, -array_get_with_nulls or -get is set", (char *)NULL);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

int
ctable_SetupAndPerformSearch (Tcl_Interp *interp, Tcl_Obj *CONST objv[], int objc, struct ctableTable *ctable) {
    struct ctableSearchStruct    search;
    CONST char                 **fieldNames = ctable->creatorTable->fieldNames;
    int                          count = ctable->count;

    if (ctable_SetupSearch (interp, objv, objc, &search, fieldNames) == TCL_ERROR) {
        return TCL_ERROR;
    }

    if (ctable_PerformSearch (interp, ctable, &search, count) == TCL_ERROR) {
        return TCL_ERROR;
    }

    if (search.components != NULL) {
	ckfree ((void *)search.components);
    }

    if (search.sortControl.fields != NULL) {
        ckfree ((void *)search.sortControl.fields);
        ckfree ((void *)search.sortControl.directions);
    }

    return TCL_OK;
}