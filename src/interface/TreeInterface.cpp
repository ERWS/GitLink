/*
 *  gitLink
 *
 *  Created by John Fultz on 6/18/14.
 *  Copyright (c) 2014 Wolfram Research. All rights reserved.
 *
 */

#include "mathlink.h"
#include "WolframLibrary.h"
#include "git2.h"
#include "RepoInterface.h"
#include "GitLinkRepository.h"
#include "GitTree.h"
#include "Message.h"
#include <climits>


EXTERN_C DLLEXPORT int GitExpandTree(WolframLibraryData libData, MLINK lnk)
{
	long argCount;
	MLCheckFunction(lnk, "List", &argCount);

	GitTree tree(lnk);
	MLExpr depth(lnk);
	int depthInt = 1;

	if (depth.isInteger())
		depthInt = depth.asInt();
	else if (depth.testSymbol("Infinity") || depth.testHead("DirectedInfinity"))
		depthInt = INT_MAX;

	if (tree.isValid())
		tree.writeContents(lnk, depthInt);
	else
	{
		tree.mlHandleError(libData, "GitExpandTree");
		MLPutSymbol(lnk, "$Failed");
	}

	return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int GitWriteTree(WolframLibraryData libData, MLINK lnk)
{
	bool success = false;
	long argCount;
	MLCheckFunction(lnk, "List", &argCount);
	MLExpr treeArgs(lnk);

	git_treebuilder* builder = NULL;
	std::string repoKey;
	for (int i = 1; i <= treeArgs.length(); i++)
	{
		MLExpr arg = treeArgs.part(i);
		MLExpr object = arg.lookupKey("Object");
		MLExpr name = arg.lookupKey("Name");
		MLExpr filemode = arg.lookupKey("FileMode");
		git_filemode_t resolvedFileMode = GIT_FILEMODE_UNREADABLE;

		if (filemode.isInteger())
			resolvedFileMode = (git_filemode_t) filemode.asInt();
		else if (filemode.isString())
		{
			if (strcmp(filemode.asString(), "Tree") == 0)
				resolvedFileMode = GIT_FILEMODE_TREE;
			else if (strcmp(filemode.asString(), "Blob") == 0)
				resolvedFileMode = GIT_FILEMODE_BLOB;
			else if (strcmp(filemode.asString(), "BlobExecutable") == 0)
				resolvedFileMode = GIT_FILEMODE_BLOB_EXECUTABLE;
			else if (strcmp(filemode.asString(), "Link") == 0)
				resolvedFileMode = GIT_FILEMODE_LINK;
		}

		if (!object.testHead("GitObject") || !name.isString() || resolvedFileMode == GIT_FILEMODE_UNREADABLE ||
			object.length() != 2 || !object.part(1).isString() || !object.part(2).testHead("GitRepo"))
		{
			MLHandleError(libData, "GitWriteTree", Message::BadTreeEntry);
			break;
		}
		GitLinkRepository repo(object.part(2));
		if (repoKey.empty())
			repoKey = repo.key();
		if (repo.key() != repoKey || !repo.isValid())
		{
			MLHandleError(libData, "GitWriteTree", Message::InconsistentRepos);
			break;
		}
		if (builder == NULL && git_treebuilder_new(&builder, repo.repo(), NULL))
		{
			MLHandleError(libData, "GitWriteTree", Message::GitOperationFailed);
			break;
		}
		if (git_treebuilder_insert(NULL, builder, name.asString(), object.asOid(), resolvedFileMode))
		{
			MLHandleError(libData, "GitWriteTree", Message::GitOperationFailed);
			break;
		}
	}

	if (builder != NULL && git_treebuilder_entrycount(builder) == treeArgs.length())
	{
		git_oid writtenTree;
		if (git_treebuilder_write(&writtenTree, builder) == 0)
		{
			MLHelper helper(lnk);
			helper.putGitObject(writtenTree, GitLinkRepository(repoKey));
			success = true;
		}
		else
			MLHandleError(libData, "GitWriteTree", Message::GitOperationFailed);
	}

	if (!success)
		MLPutSymbol(lnk, "$Failed");
	if (builder)
		git_treebuilder_free(builder);
	return LIBRARY_NO_ERROR;
}


EXTERN_C DLLEXPORT int GitDiffTrees(WolframLibraryData libData, MLINK lnk)
{
	long argCount;
	MLCheckFunction(lnk, "List", &argCount);

	GitTree tree1(lnk);
	GitTree tree2(lnk);

	MLHelper helper(lnk);
	if (tree1.isValid() && tree2.isValid())
	{
		PathSet changedFiles = tree1.getDiffPaths(tree2);
		helper.beginList();
		for (const auto& path : changedFiles)
			helper.putString(path);
		helper.endList();
	}
	else
	{
		tree1.mlHandleError(libData, "GitDiffTrees");
		tree2.mlHandleError(libData, "GitDiffTrees");
		helper.putSymbol("$Failed");
	}

	return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int GitIndexTree(WolframLibraryData libData, MLINK lnk)
{
	long argCount;
	MLCheckFunction(lnk, "List", &argCount);

	GitLinkRepository repo(lnk);

	git_index* index;
	if (!git_repository_index(&index, repo.repo()))
	{
		GitTree indexTree(repo, index);
		indexTree.write(lnk);
	}
	else
		MLPutSymbol(lnk, "$Failed");

	return LIBRARY_NO_ERROR;
}
