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

#define _NEKO_BASE_URL "https://api.nekosia.cat/api/v1"

/* TODO: enable NSFW type */
#define _WAIFU_BASE_URL "https://api.waifu.pics/sfw"


static const char *const _anime_sched_filters[] = {
	"sunday", "monday", "tuesday", "wednesday", "thursday", "friday",
	"saturday", "unknown", "other",
};


static const char *const _neko_filters[] = {
	"random", "catgirl", "foxgirl", "wolf-girl", "animal-ears", "tail", "tail-with-ribbon",
	"tail-from-under-skirt", "cute", "cuteness-is-justice", "blue-archive", "girl", "young-girl",
	"maid", "maid-uniform", "vtuber", "w-sitting", "lying-down", "hands-forming-a-heart",
	"wink", "valentine", "headphones", "thigh-high-socks", "knee-high-socks", "white-tights",
	"black-tights", "heterochromia", "uniform", "sailor-uniform", "hoodie", "ribbon", "white-hair",
	"blue-hair", "long-hair", "blonde", "blue-eyes", "purple-eyes",
};


static const char *const _waifu_filters[] = {
	"waifu", "neko", "shinobu", "megumin", "bully", "cuddle", "cry", "hug", "awoo", "kiss", "lick",
	"pat", "smug", "bonk", "yeet", "blush", "smile", "wave", "highfive", "handhold", "nom", "bite",
	"glomp", "slap", "kill", "kick", "happy", "wink", "poke", "dance", "cringe",
};


typedef struct neko {
	const char  *compressed_url;
	const char  *original_url;
	const char  *artist_username;
	const char  *artist_profile_url;
	const char  *source_url;
	const char  *anime_char_name;
	const char  *anime_name;
	const char  *category;
	json_object *json;
} Neko;


static int   _anime_sched_prep_filter(const char filter[], const char *res[]);
static int   _anime_sched_check_cache(const char filter[]);
static int   _anime_sched_fetch(const char filter[], int show_nsfw);
static int   _anime_sched_parse(ModelAnimeSched *list[], const char filter[], json_object *obj);
static void  _anime_sched_parse_list(json_object *list_obj, char *out[]);
static char *_anime_sched_build_body(const ModelAnimeSched list[], int len, int start);

static int _neko_prep_filter(const char filter[], const char *res[]);
static int _neko_fetch(Neko *n, const char filter[]);


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
	char *const body = _anime_sched_build_body(ma_list, llen, start);
	if (body == NULL)
		goto err0;

	char buff[256];
	const char *const chc_str = epoch_to_str(buff, LEN(buff), ma_list[0].created_at);
	list.title = CSTR_CONCAT("Anime Schedule: ", "\\(", filter, "\\)\nCache: ", chc_str, "\n");
	list.body = body;

	const int ret = message_list_send(&list, &pag, NULL);
	free((char *)list.title);
	free(body);

	if (ret >= 0)
		return;

err0:
	answer_callback_query_text(cmd->id_callback, "Error!", 1);
}


void
cmd_extra_neko(const CmdParam *cmd)
{
	const char *filter;
	if (_neko_prep_filter(cmd->args, &filter) < 0) {
		Str str;
		if (str_init_alloc(&str, 1024) < 0) {
			send_text_plain(cmd->msg, "Invalid argument!");
			return;
		}

		str_set(&str, "Invalid argument\\!\nAvailable arguments:`\n");
		for (int i = 0; i < (int)LEN(_neko_filters); i++)
			str_append_fmt(&str, "%s, ", _neko_filters[i]);

		str_pop(&str, 2);
		str_append_c(&str, '`');

		send_text_format(cmd->msg, str.cstr);
		str_deinit(&str);
		return;
	}

	Neko neko;
	if (_neko_fetch(&neko, filter) < 0)
		return;

	char *const source_url = tg_escape(neko.source_url);
	char *const artist = tg_escape(neko.artist_username);
	send_text_format_fmt(cmd->msg, 1,
		"`URL     :`  [Compressed](%s) \\- [Original](%s)\n"
		"`Name    : %s` from `%s`\n"
		"`Artist  :`  [%s](%s)\n"
		"`Source  :`  %s\n"
		"`Category: %s`",
		neko.compressed_url, neko.original_url,
		((neko.anime_char_name == NULL)? "[Unknown]" : neko.anime_char_name),
		((neko.anime_name == NULL)? "[Unknown]" : neko.anime_name),
		cstr_empty_if_null(artist),
		cstr_empty_if_null(neko.artist_profile_url),
		cstr_empty_if_null(source_url),
		neko.category);

	free(source_url);
	free(artist);
	json_object_put(neko.json);
}


void
cmd_extra_waifu(const CmdParam *cmd)
{
	Str str;
	const char *filter = "waifu";
	if (cstr_is_empty(cmd->args) == 0) {
		for (int i = 0; i < (int)LEN(_waifu_filters); i++) {
			if (cstr_casecmp(cmd->args, _waifu_filters[i])) {
				filter = _waifu_filters[i];
				goto out0;
			}
		}

		if (str_init_alloc(&str, 1024) < 0) {
			send_text_plain(cmd->msg, "Invalid argument!");
			return;
		}

		str_set(&str, "Invalid argument\\!\nAvailable arguments:`\n");
		for (int i = 0; i < (int)LEN(_waifu_filters); i++)
			str_append_fmt(&str, "%s, ", _waifu_filters[i]);

		str_pop(&str, 2);
		str_append_c(&str, '`');

		send_text_format(cmd->msg, str.cstr);
		str_deinit(&str);
		return;
	}

out0:
	if (str_init_alloc(&str, 1024) < 0)
		return;

	const char *const req = str_set_fmt(&str, "%s/%s", _WAIFU_BASE_URL, filter);
	if (req == NULL)
		goto out1;

	char *const res = http_send_get(req, NULL);
	if (res == NULL)
		goto out1;

	json_object *const obj = json_tokener_parse(res);
	if (obj == NULL)
		goto out2;

	json_object *url;
	if (json_object_object_get_ex(obj, "url", &url) == 00)
		goto out2;

	send_text_plain_fmt(cmd->msg, 1, "%s", json_object_get_string(url));
	json_object_put(obj);

out2:
	free(res);
out1:
	str_deinit(&str);
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
					    _ANIME_SCHED_BASE_URL, filter, bool_to_cstr(show_nsfw));
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


static char *
_anime_sched_build_body(const ModelAnimeSched list[], int len, int start)
{
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return NULL;

	start++;
	for (int i = 0; i < len; i++, start++) {
		const ModelAnimeSched *const item = &list[i];
		char *const title = tg_escape(item->title);
		str_append_fmt(&str, "*%d\\. [%s](%s)*\n", start, cstr_empty_if_null(title),
			       cstr_empty_if_null(item->url));

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

		free(title);
	}

	str_pop(&str, 1);
	return str.cstr;
}


static int
_neko_prep_filter(const char filter[], const char *res[])
{
	if (cstr_is_empty(filter)) {
		*res = "random";
		return 0;
	}

	for (int i = 0; i < (int)LEN(_neko_filters); i++) {
		if (cstr_casecmp(filter, _neko_filters[i])) {
			*res = _neko_filters[i];
			return 0;
		}
	}

	return -1;
}


static int
_neko_fetch(Neko *n, const char filter[])
{
	int ret = -1;
	Str str;
	if (str_init_alloc(&str, 1024) < 0)
		return -1;

	const char *const req = str_set_fmt(&str, "%s/images/%s", _NEKO_BASE_URL, filter);
	if (req == NULL)
		goto out0;

	char *const res = http_send_get(req, NULL);
	if (res == NULL)
		goto out0;

	memset(n, 0, sizeof(*n));
	json_object *const obj = json_tokener_parse(res);
	if (obj == NULL)
		goto out1;

	json_object *tmp_obj;
	if (json_object_object_get_ex(obj, "success", &tmp_obj) == 0)
		goto out2;

	if (json_object_get_boolean(tmp_obj) == 0)
		goto out2;

	json_object *img_obj;
	if (json_object_object_get_ex(obj, "image", &img_obj) == 0)
		goto out2;

	if (json_object_object_get_ex(img_obj, "original", &tmp_obj) == 0)
		goto out2;

	if (json_object_object_get_ex(tmp_obj, "url", &tmp_obj) == 0)
		goto out2;

	n->original_url = json_object_get_string(tmp_obj);
	if (json_object_object_get_ex(img_obj, "compressed", &tmp_obj) == 0)
		goto out2;

	if (json_object_object_get_ex(tmp_obj, "url", &tmp_obj) == 0)
		goto out2;

	n->compressed_url = json_object_get_string(tmp_obj);

	json_object *attr;
	if (json_object_object_get_ex(obj, "attribution", &attr) == 0)
		goto out2;

	json_object *source;
	if (json_object_object_get_ex(obj, "source", &source) == 0)
		goto out2;

	if (json_object_object_get_ex(source, "url", &tmp_obj))
		n->source_url = json_object_get_string(tmp_obj);

	json_object *artist;
	if (json_object_object_get_ex(attr, "artist", &artist)) {
		if (json_object_object_get_ex(artist, "username", &tmp_obj))
			n->artist_username = json_object_get_string(tmp_obj);
		if (json_object_object_get_ex(artist, "profile", &tmp_obj))
			n->artist_profile_url = json_object_get_string(tmp_obj);
	}

	json_object *anime;
	if (json_object_object_get_ex(obj, "anime", &anime) == 0)
		goto out2;

	if (json_object_object_get_ex(anime, "title", &tmp_obj))
		n->anime_name = json_object_get_string(tmp_obj);
	if (json_object_object_get_ex(anime, "character", &tmp_obj))
		n->anime_char_name = json_object_get_string(tmp_obj);

	json_object *category;
	if (json_object_object_get_ex(obj, "category", &category) == 0)
		goto out2;

	n->category = json_object_get_string(category);

	n->json = obj;
	ret = 0;

out2:
	if (ret < 0)
		json_object_put(obj);
out1:
	free(res);
out0:
	str_deinit(&str);
	return ret;
}
