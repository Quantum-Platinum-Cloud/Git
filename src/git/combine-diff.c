#include "xdiff/xmacros.h"
#include "revision.h"
	struct lline *next, *prev;
/* Lines lost from current parent (before coalescing) */
struct plost {
	struct lline *lost_head, *lost_tail;
	int len;
};

	/* Accumulated and coalesced lost lines */
	struct lline *lost;
	int lenlost;
	struct plost plost;
static int match_string_spaces(const char *line1, int len1,
			       const char *line2, int len2,
			       long flags)
{
	if (flags & XDF_WHITESPACE_FLAGS) {
		for (; len1 > 0 && XDL_ISSPACE(line1[len1 - 1]); len1--);
		for (; len2 > 0 && XDL_ISSPACE(line2[len2 - 1]); len2--);
	}

	if (!(flags & (XDF_IGNORE_WHITESPACE | XDF_IGNORE_WHITESPACE_CHANGE)))
		return (len1 == len2 && !memcmp(line1, line2, len1));

	while (len1 > 0 && len2 > 0) {
		len1--;
		len2--;
		if (XDL_ISSPACE(line1[len1]) || XDL_ISSPACE(line2[len2])) {
			if ((flags & XDF_IGNORE_WHITESPACE_CHANGE) &&
			    (!XDL_ISSPACE(line1[len1]) || !XDL_ISSPACE(line2[len2])))
				return 0;

			for (; len1 > 0 && XDL_ISSPACE(line1[len1]); len1--);
			for (; len2 > 0 && XDL_ISSPACE(line2[len2]); len2--);
		}
		if (line1[len1] != line2[len2])
			return 0;
	}

	if (flags & XDF_IGNORE_WHITESPACE) {
		/* Consume remaining spaces */
		for (; len1 > 0 && XDL_ISSPACE(line1[len1 - 1]); len1--);
		for (; len2 > 0 && XDL_ISSPACE(line2[len2 - 1]); len2--);
	}

	/* We matched full line1 and line2 */
	if (!len1 && !len2)
		return 1;

	return 0;
}

enum coalesce_direction { MATCH, BASE, NEW };

/* Coalesce new lines into base by finding LCS */
static struct lline *coalesce_lines(struct lline *base, int *lenbase,
				    struct lline *new, int lennew,
				    unsigned long parent, long flags)
{
	int **lcs;
	enum coalesce_direction **directions;
	struct lline *baseend, *newend = NULL;
	int i, j, origbaselen = *lenbase;

	if (new == NULL)
		return base;

	if (base == NULL) {
		*lenbase = lennew;
		return new;
	}

	/*
	 * Coalesce new lines into base by finding the LCS
	 * - Create the table to run dynamic programming
	 * - Compute the LCS
	 * - Then reverse read the direction structure:
	 *   - If we have MATCH, assign parent to base flag, and consume
	 *   both baseend and newend
	 *   - Else if we have BASE, consume baseend
	 *   - Else if we have NEW, insert newend lline into base and
	 *   consume newend
	 */
	lcs = xcalloc(origbaselen + 1, sizeof(int*));
	directions = xcalloc(origbaselen + 1, sizeof(enum coalesce_direction*));
	for (i = 0; i < origbaselen + 1; i++) {
		lcs[i] = xcalloc(lennew + 1, sizeof(int));
		directions[i] = xcalloc(lennew + 1, sizeof(enum coalesce_direction));
		directions[i][0] = BASE;
	}
	for (j = 1; j < lennew + 1; j++)
		directions[0][j] = NEW;

	for (i = 1, baseend = base; i < origbaselen + 1; i++) {
		for (j = 1, newend = new; j < lennew + 1; j++) {
			if (match_string_spaces(baseend->line, baseend->len,
						newend->line, newend->len, flags)) {
				lcs[i][j] = lcs[i - 1][j - 1] + 1;
				directions[i][j] = MATCH;
			} else if (lcs[i][j - 1] >= lcs[i - 1][j]) {
				lcs[i][j] = lcs[i][j - 1];
				directions[i][j] = NEW;
			} else {
				lcs[i][j] = lcs[i - 1][j];
				directions[i][j] = BASE;
			}
			if (newend->next)
				newend = newend->next;
		}
		if (baseend->next)
			baseend = baseend->next;
	}

	for (i = 0; i < origbaselen + 1; i++)
		free(lcs[i]);
	free(lcs);

	/* At this point, baseend and newend point to the end of each lists */
	i--;
	j--;
	while (i != 0 || j != 0) {
		if (directions[i][j] == MATCH) {
			baseend->parent_map |= 1<<parent;
			baseend = baseend->prev;
			newend = newend->prev;
			i--;
			j--;
		} else if (directions[i][j] == NEW) {
			struct lline *lline;

			lline = newend;
			/* Remove lline from new list and update newend */
			if (lline->prev)
				lline->prev->next = lline->next;
			else
				new = lline->next;
			if (lline->next)
				lline->next->prev = lline->prev;

			newend = lline->prev;
			j--;

			/* Add lline to base list */
			if (baseend) {
				lline->next = baseend->next;
				lline->prev = baseend;
				if (lline->prev)
					lline->prev->next = lline;
			}
			else {
				lline->next = base;
				base = lline;
			}
			(*lenbase)++;

			if (lline->next)
				lline->next->prev = lline;

		} else {
			baseend = baseend->prev;
			i--;
		}
	}

	newend = new;
	while (newend) {
		struct lline *lline = newend;
		newend = newend->next;
		free(lline);
	}

	for (i = 0; i < origbaselen + 1; i++)
		free(directions[i]);
	free(directions);

	return base;
}

		fill_filespec(df, sha1, 1, mode);
	lline->prev = sline->plost.lost_tail;
	if (lline->prev)
		lline->prev->next = lline;
	else
		sline->plost.lost_head = lline;
	sline->plost.lost_tail = lline;
	sline->plost.len++;
			 const char *path, long flags)
	xpp.flags = flags;
		/* Coalesce new lines */
		if (sline[lno].plost.lost_head) {
			struct sline *sl = &sline[lno];
			sl->lost = coalesce_lines(sl->lost, &sl->lenlost,
						  sl->plost.lost_head,
						  sl->plost.len, n, flags);
			sl->plost.lost_head = sl->plost.lost_tail = NULL;
			sl->plost.len = 0;
		}

		ll = sline[lno].lost;
	return ((sline->flag & all_mask) || sline->lost);
		while (j < i) {
			if (!(sline[j].flag & mark))
				sline[j].flag |= no_pre_delete;
			sline[j++].flag |= mark;
		}
			struct lline *ll = sline[j].lost;
static void dump_sline(struct sline *sline, const char *line_prefix,
		       unsigned long cnt, int num_parent,
		printf("%s%s", line_prefix, c_frag);
			ll = (sl->flag & no_pre_delete) ? NULL : sl->lost;
				printf("%s%s", line_prefix, c_old);
			fputs(line_prefix, stdout);
		struct lline *ll = sline->lost;
			     const char *line_prefix,
	strbuf_addstr(&buf, line_prefix);
				 const char *line_prefix,
			 "", elem->path, line_prefix, c_meta, c_reset);
	printf("%s%sindex ", line_prefix, c_meta);
			printf("%s%snew file mode %06o",
			       line_prefix, c_meta, elem->mode);
				printf("%s%sdeleted file ",
				       line_prefix, c_meta);
				 line_prefix, c_meta, c_reset);
				 line_prefix, c_meta, c_reset);
				 line_prefix, c_meta, c_reset);
				 line_prefix, c_meta, c_reset);
	const char *line_prefix = diff_line_prefix(opt);
			fill_filespec(df, null_sha1, 0, st.st_mode);
				     line_prefix, mode_differs, 0);
				     textconv, elem->path, opt->xdl_opts);
				     line_prefix, mode_differs, 1);
		dump_sline(sline, line_prefix, cnt, num_parent,
		if (sline[lno].lost) {
			struct lline *ll = sline[lno].lost;
	int line_termination, inter_name_termination, i;
	const char *line_prefix = diff_line_prefix(opt);

		printf("%s", line_prefix);

		/* As many colons as there are parents */
		for (i = 0; i < num_parent; i++)
			putchar(':');
		for (i = 0; i < num_parent; i++)
			printf("%06o ", p->parent[i].mode);
		printf("%06o", p->mode);

	copy_pathspec(&diffopts.pathspec, &opt->pathspec);

				printf("%s%c", diff_line_prefix(opt),
				       opt->line_termination);
				printf("%s%c", diff_line_prefix(opt),
				       opt->line_termination);

	free_pathspec(&diffopts.pathspec);
	struct commit_list *parent = get_saved_parents(rev, commit);