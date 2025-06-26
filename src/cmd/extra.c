#include <ctype.h>
#include <json.h>
#include <string.h>
#include <time.h>

#include "../cmd.h"
#include "../common.h"
#include "../model.h"
#include "../tg.h"
#include "../tg_api.h"


#define _ANIME_SCHED_BASE_URL       "https://api.jikan.moe/v4/schedules"
#define _ANIME_SCHED_LIMIT_SIZE     (3)
#define _ANIME_SCHED_EXPIRE         (86400) // default: 1 day

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


static int  _anime_sched_prep_filter(const char filter[], const char *res[]);
static int  _anime_sched_check_cache(const char filter[]);
static int  _anime_sched_fetch(const char filter[], int show_nsfw);
static int  _anime_sched_parse(ModelAnimeSched *list[], const char filter[], json_object *obj);
static void _anime_sched_parse_list(json_object *list_obj, char *out[]);
static int  _anime_sched_build_body(const ModelAnimeSched list[], int len, int start, char *res[]);


/*
 * Public
 */
void
cmd_extra_anime_sched(const CmdParam *cmd)
{
	MessageList list = {
		.ctx = cmd->name,
		.id_user = cmd->id_user,
		.id_owner = cmd->id_owner,
		.id_chat = cmd->id_chat,
		.id_message = cmd->msg->id,
		.id_callback = cmd->id_callback,
	};

	if (message_list_init(&list, cmd->args) < 0)
		return;

	const char *filter;
	if (_anime_sched_prep_filter(list.udata, &filter) < 0) {
		send_text_format(cmd->msg, "Invalid argument\\!\n"
					   "```Allowed:\n[sunday, monday, tuesday, wednesday, thursday, "
					   "friday, saturday, unknown, other]```");
		return;
	}

	if (_anime_sched_check_cache(filter) == 0) {
		const int cflags = model_chat_get_flags(cmd->id_chat);
		const int show_nsfw = (cflags > 0)? (cflags & MODEL_CHAT_FLAG_ALLOW_CMD_NSFW) : 0;
		if (_anime_sched_fetch(filter, show_nsfw) < 0)
			goto err0;
	} else {
		log_debug("use cache");
	}

	ModelAnimeSched ma_list[_ANIME_SCHED_LIMIT_SIZE];
	MessageListPagination pag;

	int total = 0;
	const int len = LEN(ma_list);
	const int offt = (list.page - 1) * len;
	const int llen = model_anime_sched_get_list(ma_list, len, filter, offt, &total);
	if ((llen < 0) || (total <= 0))
		goto err0;

	message_list_pagination_set(&pag, list.page, len, llen, total);

	const int start = MIN(((list.page * len) - len), pag.items_size);
	if (_anime_sched_build_body(ma_list, llen, start, (char **)&list.body) < 0)
		goto err0;

	char upd_buff[256];
	upd_buff[0] = '\0';

	time_t cre_dt = ma_list[0].created_at;
	const struct tm *const upd = localtime(&cre_dt);
	if (upd != NULL)
		cstr_copy_n(upd_buff, LEN(upd_buff), asctime(upd));

	list.title = CSTR_CONCAT("Anime Schedule: ", "\\(", filter, "\\)\nCache: ", upd_buff);

	const int ret = message_list_send(&list, &pag, NULL);
	free((char *)list.title);
	free((char *)list.body);

	if (ret >= 0)
		return;

err0:
	tg_api_answer_callback_query(cmd->id_callback, "Error!", NULL, 1);
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


/*
 *   0: expired/empty
 *  ~0: valid
 */
static int
_anime_sched_check_cache(const char filter[])
{
	time_t cre_dt;
	const int ret = model_anime_sched_get_creation_time(filter, &cre_dt);
	if (ret < 0)
		return 1;

	if (ret == 0)
		return 0;

	const time_t now = time(NULL);
	if ((now - cre_dt) >= _ANIME_SCHED_EXPIRE) {
		if (model_anime_sched_delete_by(filter) >= 0)
			return 0;
	}

	return 1;
}


static int
_anime_sched_fetch(const char filter[], int show_nsfw)
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	const char *const req = str_set_fmt(&str, "%s?filter=%s&kids=false&sfw=%s",
					    _ANIME_SCHED_BASE_URL, filter, cstr_from_bool(show_nsfw));
	if (req == NULL)
		goto out0;

	char *const res = http_send_get(req, "application/json");
	if (res == NULL)
		goto out0;

	json_object *const obj = json_tokener_parse(res);
	if (obj == NULL)
		goto out1;

	ModelAnimeSched *list;
	const int len = _anime_sched_parse(&list, filter, obj);
	if (len < 0)
		goto out2;

	ret = model_anime_sched_add_list(list, len);

	for (int i = 0; i < len; i++) {
		free((char *)list[i].genres_in);
		free((char *)list[i].themes_in);
		free((char *)list[i].demographics_in);
	}

	free(list);

out2:
	json_object_put(obj);
out1:
	free(res);
out0:
	str_deinit(&str);
	return ret;
}


static int
_anime_sched_parse(ModelAnimeSched *list[], const char filter[], json_object *obj)
{
	json_object *tmp_obj;
	array_list *data_list = NULL;
	if (json_object_object_get_ex(obj, "data", &tmp_obj))
		data_list = json_object_get_array(tmp_obj);

	if (data_list == NULL)
		return -1;

	size_t data_list_len = array_list_length(data_list);
	if (data_list_len >= INT_MAX)
		data_list_len = INT_MAX - 1;

	ModelAnimeSched *const items = malloc(sizeof(ModelAnimeSched) * data_list_len);
	if (items == NULL)
		return -1;

	for (size_t i = 0; i < data_list_len; i++) {
		json_object *const _obj = array_list_get_idx(data_list, i);
		ModelAnimeSched *const item = &items[i];

		item->filter_in = filter;
		if (json_object_object_get_ex(_obj, "mal_id", &tmp_obj))
			item->mal_id = json_object_get_int(tmp_obj);
		if (json_object_object_get_ex(_obj, "url", &tmp_obj))
			item->url_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "title", &tmp_obj))
			item->title_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "title_japanese", &tmp_obj))
			item->title_japanese_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "type", &tmp_obj))
			item->type_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "source", &tmp_obj))
			item->source_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "episodes", &tmp_obj))
			item->episodes = json_object_get_int(tmp_obj);
		if (json_object_object_get_ex(_obj, "broadcast", &tmp_obj)) {
			if (json_object_object_get_ex(tmp_obj, "string", &tmp_obj))
				item->broadcast_in = json_object_get_string(tmp_obj);
		}
		if (json_object_object_get_ex(_obj, "duration", &tmp_obj))
			item->duration_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "rating", &tmp_obj))
			item->rating_in = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(_obj, "score", &tmp_obj))
			item->score = json_object_get_double(tmp_obj);
		if (json_object_object_get_ex(_obj, "year", &tmp_obj))
			item->year = json_object_get_int(tmp_obj);

		if (json_object_object_get_ex(_obj, "genres", &tmp_obj))
			_anime_sched_parse_list(tmp_obj, (char **)&item->genres_in);
		if (json_object_object_get_ex(_obj, "themes", &tmp_obj))
			_anime_sched_parse_list(tmp_obj, (char **)&item->themes_in);
		if (json_object_object_get_ex(_obj, "demographics", &tmp_obj))
			_anime_sched_parse_list(tmp_obj, (char **)&item->demographics_in);
	}

	*list = items;
	return (int)data_list_len;
}


static void
_anime_sched_parse_list(json_object *list_obj, char *out[])
{
	array_list *const list = json_object_get_array(list_obj);
	if (list == NULL)
		return;

	Str str;
	if (str_init_alloc(&str, 128) < 0)
		return;

	const size_t list_len = array_list_length(list);
	for (size_t i = 0; i < list_len; i++) {
		json_object *const obj = array_list_get_idx(list, i);
		if (obj == NULL)
			continue;

		json_object *obj_name;
		if (json_object_object_get_ex(obj, "name", &obj_name) == 0)
			continue;

		if (str_append_fmt(&str, "%s, ", json_object_get_string(obj_name)) == NULL)
			break;
	}

	str_pop(&str, 2);

	*out = str.cstr;
}


static int
_anime_sched_build_body(const ModelAnimeSched list[], int len, int start, char *res[])
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	for (int i = 0; i < len; i++) {
		const ModelAnimeSched *const item = &list[i];
		{
			char *const title = tg_escape(item->title);
			char *const url = tg_escape(item->url);
			str_append_fmt(&str, "*%d\\. [%s](%s)*\n", (start + i + 1),
				       cstr_empty_if_null(title), cstr_empty_if_null(url));
			free(title);
			free(url);
		}

		str_append_n(&str, "```\n", 4);
		str_append_fmt(&str, "Japanese : %s\n", cstr_empty_if_null(item->title_japanese));
		str_append_fmt(&str, "Type     : %s\n", cstr_empty_if_null(item->type));
		str_append_fmt(&str, "Year     : %d\n", item->year);
		str_append_fmt(&str, "Episodes : %d\n", item->episodes);
		str_append_fmt(&str, "Source   : %s\n", cstr_empty_if_null(item->source));
		str_append_fmt(&str, "Duration : %s\n", cstr_empty_if_null(item->duration));
		str_append_fmt(&str, "Score    : %.2f\n", item->score);
		str_append_fmt(&str, "Broadcast: %s\n", cstr_empty_if_null(item->broadcast));
		str_append_fmt(&str, "Rating   : %s\n", cstr_empty_if_null(item->rating));
		str_append_fmt(&str, "Genres   : %s\n", cstr_empty_if_null(item->genres));
		str_append_fmt(&str, "Themes   : %s\n", cstr_empty_if_null(item->themes));
		str_append_fmt(&str, "Dgraphics: %s\n", cstr_empty_if_null(item->demographics));
		str_append_n(&str, "```\n", 4);
	}

	str_pop(&str, 1);
	*res = str.cstr;
	return 0;
}
