#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <locale.h>

#include "curl/curl.h"
#include "cjson/cJSON.h"
#include "config.h"

struct received{
	char *data;
	size_t len;
};

struct weather_info{
	char *min_temp;
	char *max_temp;
	char *wind_vel;
	char *wind_dir;
	int weather_type;
	//don't feel like converting stuff
	wchar_t weather_type_epic_emoji;
	wchar_t wind_dir_epic_emoji;
};

static void print_weather_info(struct weather_info *weather){
	printf("Min temp: %s\nMax temp: %s\nWind Dir: %s\nWind Vel: %s\nWeather Type: %d\n",
		   weather->min_temp, weather->max_temp, weather->wind_dir, weather->wind_vel, weather->weather_type);
}

static void replace_with_epic_emojis(struct weather_info *weather_info){
	//TODO change to a map
	//weather type
	switch (weather_info->weather_type) {
		case 1:
			weather_info->weather_type_epic_emoji = 0x1F31E;
			break;
		case 2:
			weather_info->weather_type_epic_emoji = 0x1F324;
			break;
		case 3 ... 5:
			weather_info->weather_type_epic_emoji = 0x1F324;
			break;
		case 6 ... 7:
			weather_info->weather_type_epic_emoji = 0x1F326;
			break;
		case 8 ... 9:
			weather_info->weather_type_epic_emoji = 0x1F327;
			break;
		case 10 ... 13:
			weather_info->weather_type_epic_emoji = 0x1F326;
			break;
		case 14:
			weather_info->weather_type_epic_emoji = 0x1F327;
			break;
		case 15:
			weather_info->weather_type_epic_emoji = 0x1F326;
			break;
		case 27:
			weather_info->weather_type_epic_emoji = 0x2601;
			break;
		default:
			weather_info->weather_type_epic_emoji = 0x1F468;
			break;
	}
	//wind direction
	/*switch (weather_info->wind_dir) {
		case "N":
			weather_info->wind_dir_epic_emoji = 0x2B06;
			break;
		case "NE":
			weather_info->wind_dir_epic_emoji = 0x2189;
			break;
		case "E":
			weather_info->wind_dir_epic_emoji = 0x27A1;
			break;
		case "SE":
			weather_info->wind_dir_epic_emoji = 0x2198;
			break;
		case "S":
			weather_info->wind_dir_epic_emoji = 0x2B07;
			break;
		case "SW":
			weather_info->wind_dir_epic_emoji = 0x2199;
			break;
		case "W":
			weather_info->wind_dir_epic_emoji = 0x2B05;
			break;
		case "NW":
			weather_info->wind_dir_epic_emoji = 0x2196;
			break;
		default:
			weather_info->wind_dir_epic_emoji = 0x1F468;
			break;
	}*/
}

static size_t writefunc(void *ptr, size_t size, size_t nmemb, void* userdata){
	struct received *temp_userdata = (struct received*) userdata;
	size_t new_len = temp_userdata->len + size * nmemb;
	temp_userdata->data = realloc(temp_userdata->data, new_len + 1);
	if (temp_userdata->data == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(temp_userdata->data + temp_userdata->len, ptr, size * nmemb);
	temp_userdata->data[new_len] = '\0';
	temp_userdata->len = new_len;

	return size*nmemb; 
}

static void remove_year_month_day(char *date_from_json, char hour_min_sec_date[]){
	int divider_index = 12;
	int i = 0;
	int length = strlen(date_from_json);
	while (i < length) {
		hour_min_sec_date[i] = date_from_json[divider_index+i-1];
		i++;
	}
	hour_min_sec_date[i] = '\0';
}

static void get_city_weather_info(cJSON *json, struct weather_info *weather_info){
	const cJSON *temp = cJSON_GetArrayItem(json, 0);
	const cJSON *min_temp_data = cJSON_GetObjectItemCaseSensitive(temp, "tMin");
	const cJSON *max_temp_data = cJSON_GetObjectItemCaseSensitive(temp, "tMax");
	weather_info->min_temp = cJSON_GetStringValue(min_temp_data);
	weather_info->max_temp = cJSON_GetStringValue(max_temp_data);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	int current_hour = t->tm_hour;

	cJSON_ArrayForEach(temp, json){
		cJSON *temp_date = cJSON_GetObjectItemCaseSensitive(temp, "dataPrev");
		char *date_from_json = cJSON_GetStringValue(temp_date);
		char hour_min_sec_date[20];
		remove_year_month_day(date_from_json, hour_min_sec_date);
		char *split = strtok(hour_min_sec_date, ":");
		if(current_hour == atoi(split)){
			const cJSON *wind_vel = cJSON_GetObjectItemCaseSensitive(temp, "ffVento");
			weather_info->wind_vel = cJSON_GetStringValue(wind_vel);
			const cJSON *wind_dir = cJSON_GetObjectItemCaseSensitive(temp, "ddVento");
			weather_info->wind_dir = cJSON_GetStringValue(wind_dir);
			const cJSON *weather_type = cJSON_GetObjectItemCaseSensitive(temp, "idTipoTempo");
			weather_info->weather_type = weather_type->valueint;

			break;
		}
	}
}

void get_city_globalIdLocal(const cJSON *json, char *local, int *id){
	const cJSON *temp = NULL;
    cJSON_ArrayForEach(temp, json){
		cJSON *temp_local = cJSON_GetObjectItemCaseSensitive(temp, "local");
		if(strncmp(local, temp_local->valuestring, strlen(local)) == 0){
			const cJSON *temp_id = cJSON_GetObjectItemCaseSensitive(temp, "globalIdLocal");
			*id = temp_id->valueint;
			return;
		}
	}
}


int main(void){
	setlocale(LC_ALL, "en_US.utf8");
	CURL *curl;
	CURLcode res;
	struct received json_as_string;
	json_as_string.data = NULL;
	json_as_string.len = 0;

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipma.pt/public-data/forecast/locations.json");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_as_string);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		cJSON *json = cJSON_Parse(json_as_string.data);
		if (json == NULL){
			const char *error_ptr = cJSON_GetErrorPtr();
			if (error_ptr != NULL){
				fprintf(stderr, "Error before: %s\n", error_ptr);
			}
			cJSON_Delete(json);
		}

		int globalId;
		get_city_globalIdLocal(json, CITY, &globalId);
		json_as_string.data = NULL;
		json_as_string.len = 0;

		//get weather info
		char globalId_str[8];
		snprintf(globalId_str, sizeof(globalId_str) / sizeof(globalId_str[0]), "%d", globalId);
		size_t globalId_str_len = strlen(globalId_str);
		char *filetype = ".json";
		size_t filetype_len = strlen(filetype);
		char *forecast = "https://api.ipma.pt/public-data/forecast/aggregate/";
		size_t forecast_len = strlen(forecast);
		char forecast_url[forecast_len + globalId_str_len + filetype_len + 1];
		strcpy(forecast_url, forecast);
		strcat(forecast_url, globalId_str);
		strcat(forecast_url, filetype);

		curl_easy_setopt(curl, CURLOPT_URL, forecast_url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_as_string);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		json = cJSON_Parse(json_as_string.data);
		if (json == NULL){
			const char *error_ptr = cJSON_GetErrorPtr();
			if (error_ptr != NULL){
				fprintf(stderr, "Error before: %s\n", error_ptr);
			}
			cJSON_Delete(json);
		}

		struct weather_info weather;
		get_city_weather_info(json, &weather);
		replace_with_epic_emojis(&weather);
		printf("%lc %s %s %s %s", weather.weather_type_epic_emoji, weather.min_temp,
								 weather.max_temp, weather.wind_dir, weather.wind_vel);

		//clean stuff
		cJSON_Delete(json);
		curl_easy_cleanup(curl);
	}
	return 0;
}
