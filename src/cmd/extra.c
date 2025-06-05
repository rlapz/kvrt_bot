#include <ctype.h>
#include <json.h>
#include <string.h>
#include <time.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../tg.h"
#include "../tg_api.h"


#define _ANIME_SCHED_ITEM_LIST_SIZE (16)
#define _ANIME_SCHED_BASE_URL       "https://api.jikan.moe/v4/schedules"
#define _ANIME_SCHED_LIMIT_SIZE     (3)

static const char *const _anime_sched_filters[] = {
	"sunday",
	"monday",
	"tuesday",
	"wednesday",
	"thursday",
	"friday",
	"saturday",
	"unknown",
	"other",
};


typedef struct anime_sched_item {
	const char *url;
	const char *title;
	const char *title_japanese;
	const char *type;
	const char *source;
	unsigned    episodes;
	const char *status;
	const char *duration;
	const char *rating;
	double      score;
	unsigned    year;
	const char *genres[_ANIME_SCHED_ITEM_LIST_SIZE];		/* NULL-terminated */
	const char *themes[_ANIME_SCHED_ITEM_LIST_SIZE];		/* NULL-terminated */
	const char *demographics[_ANIME_SCHED_ITEM_LIST_SIZE];		/* NULL-terminated */
} AnimeSchedItem;

typedef struct anime_sched {
	MessageListPagination pagination;
	AnimeSchedItem        items[CFG_LIST_ITEMS_SIZE];
} AnimeSched;

static int  _anime_sched_prep_filter(const char filter[], const char *res[]);
static int  _anime_sched_get_list(const char filter[], unsigned limit, unsigned page,
				  int show_nsfw, json_object **res);
static int  _anime_sched_parse(AnimeSched *a, json_object *obj);
static void _anime_sched_parse_list(json_object *list_obj, const char *out[], size_t len);
static int  _anime_sched_build_body(AnimeSched *a, unsigned start, char *res[]);


/*
 * Public
 */
void
cmd_extra_anime_sched(const CmdParam *cmd)
{
	unsigned page_num;
	const char *udata;
	const TgMessage *const msg = cmd->msg;
	const char *const cb_id = (cmd->callback != NULL)? cmd->callback->id : NULL;
	if (message_list_prep(msg->chat.id, msg->id, cb_id, cmd->args, &page_num, &udata) < 0)
		return;

	const char *filter;
	if (_anime_sched_prep_filter(udata, &filter) < 0) {
		send_text_plain(cmd->msg, "Invalid argument!\n"
					  "  Allowed: [sunday, monday, tuesday, wednesday, thursday, "
					  "friday, saturday, unknown, other]");
		return;
	}

	const int cflags = model_chat_get_flags(cmd->msg->chat.id);
	const int show_nsfw = (cflags > 0)? (cflags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW) : 0;
	const unsigned limit = MIN(_ANIME_SCHED_LIMIT_SIZE, CFG_LIST_ITEMS_SIZE);

	json_object *obj;
	if (_anime_sched_get_list(filter, limit, page_num, show_nsfw, &obj) < 0)
		return;

	AnimeSched anime;
	if (_anime_sched_parse(&anime, obj) < 0)
		goto out0;

	char *body;
	const unsigned start = MIN(((page_num * limit) - limit), anime.pagination.items_size);
	if (_anime_sched_build_body(&anime, start, &body) < 0)
		goto out0;

	char *const title = CSTR_CONCAT("Anime Schedule: ", "\\(", filter, "\\)");
	MessageList mlist = {
		.ctx = cmd->name,
		.msg = cmd->msg,
		.title = (title != NULL)? title : "Anime Schedule",
		.body = body,
		.udata = filter,
		.is_edit = (cmd->callback != NULL),
	};

	message_list_send(&mlist, &anime.pagination, NULL);
	free(body);
	free(title);

out0:
	json_object_put(obj);
}


/*
 * Private
 */
static int
_anime_sched_prep_filter(const char filter[], const char *res[])
{
        char name[16];
	if (cstr_is_empty(filter)) {
		time_t t = time(NULL);
		struct tm *const tm = localtime(&t);
		if (tm == NULL)
			return -1;

		if (strftime(name, sizeof(name), "%A", tm) == 0)
			return -1;

		filter = name;
	}

	for (size_t i = 0; i < LEN(_anime_sched_filters); i++) {
		const char *const ret = _anime_sched_filters[i];
		if (cstr_casecmp(ret, filter)) {
			*res = ret;
			return 0;
		}
	}

	return -1;
}


static int
_anime_sched_get_list(const char filter[], unsigned limit, unsigned page, int show_nsfw, json_object **res)
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return ret;

	const char *const req = str_set_fmt(&str, "%s?page=%u&filter=%s&limit=%u&kids=false&swf=%s",
					    _ANIME_SCHED_BASE_URL, page, filter, limit,
					    ((show_nsfw)? "true" : "false"));
	if (req == NULL)
		goto out0;

	char *const result = http_send_get(req, "application/json");
	if (result == NULL)
		goto out0;

	json_object *const obj = json_tokener_parse(result);
	if (obj != NULL) {
		*res = obj;
		ret = 0;
	}

	free(result);

out0:
	str_deinit(&str);
	return ret;
}


static int
_anime_sched_parse(AnimeSched *a, json_object *obj)
{
	unsigned per_page = 0;
	json_object *tmp_obj;
	memset(a, 0, sizeof(AnimeSched));

	json_object *pg_obj;
	if (json_object_object_get_ex(obj, "pagination", &pg_obj) == 0)
		return -1;

	array_list *data_list = NULL;
	if (json_object_object_get_ex(obj, "data", &tmp_obj))
		data_list = json_object_get_array(tmp_obj);

	if (data_list == NULL)
		return -1;

	if (json_object_object_get_ex(pg_obj, "has_next_page", &tmp_obj) == 0)
		return -1;

	a->pagination.has_next_page = json_object_get_boolean(tmp_obj);
	if (json_object_object_get_ex(pg_obj, "current_page", &tmp_obj) == 0)
		return -1;

	a->pagination.page_count = (unsigned)json_object_get_uint64(tmp_obj);

	json_object *items_obj;
	if (json_object_object_get_ex(pg_obj, "items", &items_obj) == 0)
		return -1;

	if (json_object_object_get_ex(items_obj, "count", &tmp_obj) == 0)
		return -1;

	a->pagination.items_count = (unsigned)json_object_get_uint64(tmp_obj);
	if (json_object_object_get_ex(items_obj, "total", &tmp_obj) == 0)
		return -1;

	a->pagination.items_size = (unsigned)json_object_get_uint64(tmp_obj);
	if (json_object_object_get_ex(items_obj, "per_page", &tmp_obj) == 0)
		return -1;

	per_page = (unsigned)json_object_get_uint64(tmp_obj);

	const size_t data_list_len = array_list_length(data_list);
	if (data_list_len >= CFG_LIST_ITEMS_SIZE)
		return -1;

	for (size_t i = 0; i < data_list_len; i++) {
		json_object *const _obj = array_list_get_idx(data_list, i);
		AnimeSchedItem *const item = &a->items[i];
		if (json_object_object_get_ex(_obj, "url", &tmp_obj))
			item->url = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "title", &tmp_obj))
			item->title = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "title_japanese", &tmp_obj))
			item->title_japanese = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "type", &tmp_obj))
			item->type = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "source", &tmp_obj))
			item->source = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "episodes", &tmp_obj))
			item->episodes = (unsigned)json_object_get_int(tmp_obj);
		if (json_object_object_get_ex(_obj, "status", &tmp_obj))
			item->status = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "duration", &tmp_obj))
			item->duration = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "rating", &tmp_obj))
			item->rating = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "score", &tmp_obj))
			item->score = json_object_get_double(tmp_obj);
		if (json_object_object_get_ex(_obj, "year", &tmp_obj))
			item->year = (unsigned)json_object_get_int(tmp_obj);
		if (json_object_object_get_ex(_obj, "genres", &tmp_obj))
			_anime_sched_parse_list(tmp_obj, item->genres, _ANIME_SCHED_ITEM_LIST_SIZE);
		if (json_object_object_get_ex(_obj, "themes", &tmp_obj))
			_anime_sched_parse_list(tmp_obj, item->themes, _ANIME_SCHED_ITEM_LIST_SIZE);
		if (json_object_object_get_ex(_obj, "demographics", &tmp_obj))
			_anime_sched_parse_list(tmp_obj, item->demographics, _ANIME_SCHED_ITEM_LIST_SIZE);
	}

	a->pagination.page_size = (unsigned)ceil(((double)a->pagination.items_size) / ((double)per_page));
	return 0;
}


static void
_anime_sched_parse_list(json_object *list_obj, const char *out[], size_t len)
{
	if (len == 0)
		return;

	array_list *const list = json_object_get_array(list_obj);
	if (list == NULL)
		return;

	len--; /* reserve + 1 for NULL terminated pointer */
	const size_t list_len = array_list_length(list);
	for (size_t i = 0, j = 0; (i < list_len) && (j < len); i++) {
		json_object *const obj = array_list_get_idx(list, i);
		if (obj == NULL)
			continue;

		json_object *obj_name;
		if (json_object_object_get_ex(obj, "name", &obj_name) == 0)
			continue;

		out[j++] = json_object_get_string(obj_name);
	}
}


static int
_anime_sched_build_body(AnimeSched *a, unsigned start, char *res[])
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	for (unsigned i = 0; i < a->pagination.items_count; i++) {
		const AnimeSchedItem *const item = &a->items[i];
		{
			char *const title = tg_escape(item->title);
			char *const url = tg_escape(item->url);
			str_append_fmt(&str, "*%u\\. [%s](%s)*\n", (start + i + 1),
				       cstr_empty_if_null(title), cstr_empty_if_null(url));
			free(title);
			free(url);
		}

		str_append_n(&str, "```\n", 4);
		str_append_fmt(&str, "Japanese : %s\n", cstr_empty_if_null(item->title_japanese));
		str_append_fmt(&str, "Type     : %s\n", cstr_empty_if_null(item->type));
		str_append_fmt(&str, "Year     : %u\n", item->year);
		str_append_fmt(&str, "Episodes : %u\n", item->episodes);
		str_append_fmt(&str, "Source   : %s\n", cstr_empty_if_null(item->source));
		str_append_fmt(&str, "Duration : %s\n", cstr_empty_if_null(item->duration));
		str_append_fmt(&str, "Score    : %.2f\n", item->score);
		str_append_fmt(&str, "Status   : %s\n", cstr_empty_if_null(item->status));
		str_append_fmt(&str, "Rating   : %s\n", cstr_empty_if_null(item->rating));

		const char *const *tmp = item->genres;
		if (*tmp != NULL) {
			log_debug("%s", *tmp);
			str_append_fmt(&str, "Genres   : ");
			while (*tmp != NULL)
				str_append_fmt(&str, "%s, ", (*tmp++));

			str_pop(&str, 2);
			str_append_c(&str, '\n');
		}

		tmp = item->themes;
		if (*tmp != NULL) {
			str_append_fmt(&str, "Themes   : ");
			while (*tmp != NULL)
				str_append_fmt(&str, "%s, ", (*tmp++));

			str_pop(&str, 2);
			str_append_c(&str, '\n');
		}

		tmp = item->demographics;
		if (*tmp != NULL) {
			str_append_fmt(&str, "Dgraphics: ");
			while (*tmp != NULL)
				str_append_fmt(&str, "%s, ", (*tmp++));

			str_pop(&str, 2);
			str_append_c(&str, '\n');
		}

		str_append_n(&str, "```\n", 4);
	}

	str_pop(&str, 1);
	*res = str.cstr;
	return 0;
}
