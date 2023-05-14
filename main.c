#include <stdio.h>
#include "json.h"

char *read_request_file()
{
	char *buffer = 0;
	long length;
	FILE *f = fopen("../request.json", "rb");

	if (f)
	{
		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);
		buffer = malloc(length);
		if (buffer)
		{
			fread(buffer, 1, length, f);
		}
		fclose(f);
	}

	return buffer;
}

typedef struct
{
	int teams;
	int matches_per_session;
	int sessions;
} request;

typedef struct
{
	int id;
	int teams[4];
	int pairs[6];
	int trios[4];
} match;

typedef struct
{
	int teams[2];
} pair;

typedef struct
{
	request request;
	match *match_options;
	int match_options_size;
	pair *pairings_list;
	int pairings_list_size;
	int active_depth;
	int total_matches;
} search_metadata;

int read_int(json_object *json, const char *key)
{
	json_object *reader;
	json_object_object_get_ex(json, key, &reader);
	return json_object_get_int(reader);
}

void read_request(request *request)
{
	char *request_string = read_request_file();
	printf("%s\n", request_string);

	json_object *request_json = json_tokener_parse(request_string);

	request->teams = read_int(request_json, "teams");
	request->matches_per_session = read_int(request_json, "matchesPerSession");
	request->sessions = read_int(request_json, "sessions");

	json_object_put(request_json);
	free(request_string);
}

long factorial(int n)
{
	long i = 1;
	while (n > 0)
	{
		i *= n;
		n--;
	}
	return i;
}

long choose(int n, int k)
{
	return factorial(n) / (factorial(k) * factorial(n - k));
}

typedef struct
{
	int *team_tally;
	int team_score;
	int *pairs_tally;
	int pair_score;
} score_metadata;

void create_score_metadata(score_metadata *target, search_metadata const *search_metadata)
{
	target->team_tally = malloc(sizeof(*target->team_tally) * search_metadata->request.teams);
	for (int i = 0; i < search_metadata->request.teams; i++)
	{
		target->team_tally[i] = 0;
	}

	target->pairs_tally = malloc(sizeof(*target->pairs_tally) * search_metadata->pairings_list_size);
	for (int i = 0; i < search_metadata->pairings_list_size; i++)
	{
		target->pairs_tally[i] = 0;
	}
}

void clone_score_metadata(score_metadata *target, score_metadata const *origin, search_metadata const *search_metadata)
{

	target->team_tally = malloc(sizeof(*target->team_tally) * search_metadata->request.teams);

	for (int i = 0; i < search_metadata->request.teams; i++)
	{
		target->team_tally[i] = origin->team_tally[i];
	}

	target->pairs_tally = malloc(sizeof(*target->pairs_tally) * search_metadata->pairings_list_size);
	for (int i = 0; i < search_metadata->pairings_list_size; i++)
	{
		target->pairs_tally[i] = origin->pairs_tally[i];
	}
}

void free_score_metadata(score_metadata *target)
{
	free(target->team_tally);
	target->team_tally = NULL;

	free(target->pairs_tally);
	target->pairs_tally = NULL;
}

typedef struct node node;
struct node
{
	int depth;
	int children_size;
	char *match_availability;
	node *parent;
	match *match;
	node *children;
	node **last_searched_child;
	node **sorted_children;
	score_metadata score_metadata;
	int score;
};

char const *describe_match(match *match)
{
	char const *buffer = malloc(sizeof(*buffer) * 12);
	sprintf(buffer,
			"%d|%d|%d|%d",
			match->teams[0],
			match->teams[1],
			match->teams[2],
			match->teams[3]);
	return buffer;
}

int flat_score_func(
	int *score_set,
	int score_set_size,
	int *increment_set,
	int increment_set_size,
	int min)
{
	int team_scan = 0;
	int max = 0;

	for (int i = 0; i < score_set_size; i++)
	{
		if (team_scan < increment_set_size && increment_set[team_scan] == i)
		{
			team_scan++;
			score_set[i]++;
		}
		if (score_set[i] > max)
		{
			max = score_set[i];
		}

		if (score_set[i] < min)
		{
			min = score_set[i];
		}
	}
	return max - min;
}

void score(node *active_node, search_metadata const *search_metadata)
{

	clone_score_metadata(
		&active_node->score_metadata,
		&active_node->parent->score_metadata,
		search_metadata);

	active_node->score = 0;
	active_node->score_metadata.team_score = flat_score_func(active_node->score_metadata.team_tally,
															 search_metadata->request.teams,
															 active_node->match->teams,
															 4,
															 search_metadata->total_matches);
	active_node->score_metadata.pair_score = flat_score_func(active_node->score_metadata.pairs_tally,
															 search_metadata->pairings_list_size,
															 active_node->match->pairs,
															 6,
															 search_metadata->total_matches);
	active_node->score = active_node->score_metadata.pair_score;
}

node **create_trail(node *leaf_node, int length)
{
	if (!length)
	{
		return NULL;
	}
	node **trail = malloc(sizeof(*trail) * length);
	while (length > 0)
	{
		trail[length - 1] = leaf_node;
		leaf_node = leaf_node->parent;
		length--;
	}
	return trail;
}

void TopDownSplitMerge(node **B, int iBegin, int iEnd, node **A, int compare(node *a, node *b))
{
	if (iEnd - iBegin <= 1)
		return;

	int iMiddle = (iEnd + iBegin) / 2; // iMiddle = mid point
	// recursively sort both runs from array A[] into B[]
	TopDownSplitMerge(A, iBegin, iMiddle, B, compare); // sort the left  run
	TopDownSplitMerge(A, iMiddle, iEnd, B, compare);   // sort the right run
	// merge the resulting runs from array B[] into A[]

	int i = iBegin, j = iMiddle;

	// While there are elements in the left or right runs...
	for (int k = iBegin; k < iEnd; k++)
	{
		// If left run head exists and is <= existing right run head.
		if (i < iMiddle && (j >= iEnd || compare(B[i], B[j]) < 0))
		{
			A[k] = B[i];
			i = i + 1;
		}
		else
		{
			A[k] = B[j];
			j = j + 1;
		}
	}
}

int compare_team_score(node *a, node *b)
{
	return b->score_metadata.team_score - a->score_metadata.team_score;
}

int compare_pair_score(node *a, node *b)
{
	return b->score_metadata.pair_score - a->score_metadata.pair_score;
}

void sort(node const **nodes, int size, int compare(node *a, node *b))
{
	node const **sorting_nodes = malloc(sizeof(*sorting_nodes) * size);
	for (int i = 0; i < size; i++)
	{
		sorting_nodes[i] = nodes[i];
	}

	TopDownSplitMerge(sorting_nodes, 0, size, nodes, compare);
	free(sorting_nodes);
}

void create_children(node *parent_node, search_metadata const *search_metadata)
{
	parent_node->depth = search_metadata->active_depth;

	parent_node->match_availability = malloc(sizeof(*parent_node->match_availability) * search_metadata->match_options_size);
	if (parent_node->parent)
	{
		memcpy(
			parent_node->match_availability,
			parent_node->parent->match_availability,
			sizeof(*parent_node->match_availability) * search_metadata->match_options_size);
		parent_node->match_availability[parent_node->match->id] = 0;
	}
	else
	{
		memset(
			parent_node->match_availability,
			1,
			sizeof(*parent_node->match_availability) * search_metadata->match_options_size);
	}

	// init children

	parent_node->children_size = 0;
	for (int i = 0; i < search_metadata->match_options_size; i++)
	{
		if (parent_node->match_availability[i])
		{
			parent_node->children_size++;
		}
	}

	if (parent_node->children_size == 0)
	{
		return;
	}

	parent_node->children = malloc(sizeof(*parent_node->children) * parent_node->children_size);
	for (
		int option_index = 0, child_index = 0;
		option_index < search_metadata->match_options_size;
		option_index++)
	{
		if (!parent_node->match_availability[option_index])
		{
			continue;
		}
		node *child = parent_node->children + child_index;
		child->parent = parent_node;
		child->match = search_metadata->match_options + option_index;
		child->sorted_children = NULL;
		score(child, search_metadata);

		child_index++;
		if (child_index > parent_node->children_size)
		{
			printf("Children create sanity check failed\n");
			fflush(stdout);
			exit(1);
		}
	}

	// init sorted children

	parent_node->sorted_children = malloc(sizeof(*parent_node->sorted_children) * parent_node->children_size);
	for (int i = 0; i < parent_node->children_size; i++)
	{
		parent_node->sorted_children[i] = parent_node->children + i;
	}

	sort(parent_node->sorted_children, parent_node->children_size, compare_pair_score);
	sort(parent_node->sorted_children, parent_node->children_size, compare_team_score);

	parent_node->last_searched_child = parent_node->sorted_children + parent_node->children_size;
}

void write_result(node const *result_node, search_metadata const *search_metadata)
{
	node **trail = create_trail(result_node, search_metadata->active_depth);
	json_object *solution_json = json_object_new_object();
	json_object *solution_array = json_object_new_array_ext(search_metadata->active_depth);
	json_object_object_add(solution_json, "solution", solution_array);
	for (int i = 0; i < search_metadata->active_depth; i++)
	{
		char *desc = describe_match(trail[i]->match);
		json_object_array_add(solution_array,
							  json_object_new_string(desc));
		free(desc);
	}
	free(trail);

	json_object *score_json = json_object_new_object();
	json_object_object_add(solution_json, "score", score_json);

	json_object_object_add(score_json, "total", json_object_new_int(result_node->score));

	json_object *score_metadata_json = json_object_new_object();
	json_object_object_add(score_json, "metadata", score_metadata_json);

	json_object *team_appearance_map_json = json_object_new_object();
	json_object_object_add(score_metadata_json, "teams", team_appearance_map_json);

	for (int i = 0; i < search_metadata->request.teams; i++)
	{
		char buffer[3];
		sprintf(buffer, "%d", i);
		json_object_object_add(team_appearance_map_json, buffer, json_object_new_int(result_node->score_metadata.team_tally[i]));
	}

	json_object *pairings_map_json = json_object_new_object();
	json_object_object_add(score_metadata_json, "pairings", pairings_map_json);

	for (int i = 0; i < search_metadata->pairings_list_size; i++)
	{
		char buffer[6];
		int *pair_teams = search_metadata->pairings_list[i].teams;
		sprintf(buffer, "%d|%d", pair_teams[0], pair_teams[1]);
		json_object_object_add(pairings_map_json, buffer, json_object_new_int(result_node->score_metadata.pairs_tally[i]));
	}

	char const *solution_string = json_object_to_json_string_ext(
		solution_json,
		JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_PRETTY_TAB);

	FILE *file = fopen("solution.json", "w");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}

	fprintf(file, solution_string);
	fclose(file);

	json_object_put(solution_json);
}

void free_node(node *node)
{
	free(node->match_availability);
	if (node->children_size == 0)
	{
		return;
	}
	for (int i = 0; i < node->children_size; i++)
	{
		free_score_metadata(&node->children[i].score_metadata);
	}
	free(node->children);
	free(node->sorted_children);
}

void mark_expored(node *node)
{
	if (!node->parent)
	{
		return;
	}

	node->parent->match_availability[node->match->id] = 1;
}

int main()
{
	search_metadata search_metadata;
	read_request(&search_metadata.request);
	printf("%d\n", search_metadata.request.teams);

	search_metadata.active_depth = 0;
	search_metadata.match_options_size = choose(search_metadata.request.teams, 4);

	printf("match options length: %d\n", search_metadata.match_options_size);
	fflush(stdout);

	search_metadata.match_options = malloc(sizeof(*search_metadata.match_options) * search_metadata.match_options_size);

	search_metadata.pairings_list_size = choose(search_metadata.request.teams, 2);
	printf("pairings_list_size: %d\n", search_metadata.pairings_list_size);
	search_metadata.pairings_list = malloc(sizeof(*search_metadata.pairings_list) * search_metadata.pairings_list_size);

	for (int i = 0; i < search_metadata.match_options_size; i++)
	{
		search_metadata.match_options[i].id = i;
	}

	int pair_id = 0;
	int **pair_map = malloc(sizeof(*pair_map) * search_metadata.request.teams);
	for (int i = 0; i < search_metadata.request.teams; i++)
	{
		pair_map[i] = malloc(sizeof(**pair_map) * search_metadata.request.teams);
		for (int j = i + 1; j < search_metadata.request.teams; j++)
		{
			pair_map[i][j] = pair_id;
			search_metadata.pairings_list[pair_id]
				.teams[0] = i;
			search_metadata.pairings_list[pair_id].teams[1] = j;
			pair_id++;
		}
	}

	match *last_match = search_metadata.match_options;
	for (int i = 0; i < search_metadata.request.teams; i++)
	{
		for (int j = i + 1; j < search_metadata.request.teams; j++)
		{
			for (int k = j + 1; k < search_metadata.request.teams; k++)
			{
				for (int l = k + 1; l < search_metadata.request.teams; l++)
				{
					last_match->teams[0] = i;
					last_match->teams[1] = j;
					last_match->teams[2] = k;
					last_match->teams[3] = l;

					last_match->pairs[0] = pair_map[i][j];
					last_match->pairs[1] = pair_map[i][k];
					last_match->pairs[2] = pair_map[i][l];
					last_match->pairs[3] = pair_map[j][k];
					last_match->pairs[4] = pair_map[j][l];
					last_match->pairs[5] = pair_map[k][l];

					last_match++;
				}
			}
		}
	}

	for (int i = 0; i < search_metadata.request.teams; i++)
	{
		free(pair_map[i]);
	}
	free(pair_map);

	search_metadata.total_matches = search_metadata.request.matches_per_session * search_metadata.request.sessions;
	node *active_node = malloc(sizeof(*active_node));

	active_node->parent = NULL;
	active_node->sorted_children = NULL;
	active_node->score = -1;

	create_score_metadata(&active_node->score_metadata, &search_metadata);
	printf("initial node %d\n", active_node);

	printf("total_matches %d\n", search_metadata.total_matches);

	int best_score = 1;
	int processed_nodes = 0;
	int steps = 0;

	while (active_node != NULL)
	{
		// if (processed_nodes / 10000 > 0)
		// {
		// 	steps += (processed_nodes / 10000);
		// 	processed_nodes %= 10000;
		// 	// printf("%d\r", steps);
		// }

		if (search_metadata.active_depth == search_metadata.total_matches)
		{
			if (active_node->score < best_score)
			{
				best_score = active_node->score;
				printf("New best score %d\n", best_score);
				fflush(stdout);

				write_result(active_node, &search_metadata);
			}

			if (active_node->score <= 1)
			{
				break;
			}

			mark_expored(active_node);
			search_metadata.active_depth--;
			active_node = active_node->parent;

			continue;
		}

		if (active_node->sorted_children == NULL)
		{
			// printf("create: parent(%x) active(%x), d(%d)\n", active_node->parent, active_node, search_metadata.active_depth);
			processed_nodes += active_node->children_size;
			create_children(active_node, &search_metadata);
			printf("%d\r", search_metadata.active_depth);
		}

		if (active_node->children_size == 0)
		{
			mark_expored(active_node);
			free_node(active_node);
			search_metadata.active_depth--;
			active_node = active_node->parent;
			continue;
		}

		if (active_node->last_searched_child == active_node->sorted_children)
		{
			free_node(active_node);
			mark_expored(active_node);
			search_metadata.active_depth--;
			active_node = active_node->parent;
			continue;
		}

		active_node->last_searched_child--;
		if ((**active_node->last_searched_child).score_metadata.team_score > 1 ||
			(**active_node->last_searched_child).score > best_score)
		{
			mark_expored(active_node);
			free_node(active_node);
			search_metadata.active_depth--;
			active_node = active_node->parent;
			continue;
		}
		search_metadata.active_depth++;
		active_node = *active_node->last_searched_child;
	}

	return 0;
}
