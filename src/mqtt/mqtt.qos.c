
#include <tr50/util/memory.h>
#include <tr50/mqtt/mqtt.h>
#include <tr50/util/mutex.h>
#include <tr50/util/time.h>
#include <tr50/util/platform.h>
#include <tr50/error.h>

typedef struct {
	int is_used;
	unsigned short msg_id;
	mqtt_qos_callback callback;
	void *custom;
	int expiration_timestamp;
} _MQTT_QOS_RECORD;

typedef struct {
	int max_size;
	int current_size;
	_MQTT_QOS_RECORD *records;
	void *mux;
	int timeout_in_sec;
} _MQTT_QOS;

int mqtt_qos_create(void **mqtt_qos_handle, unsigned char size, int timeout_in_sec) {
	_MQTT_QOS *mq;
	if ((mq = _memory_malloc(sizeof(_MQTT_QOS))) == NULL) return ERR_TR50_MALLOC;
	_memory_memset(mq, 0, sizeof(_MQTT_QOS));

	if ((mq->records = _memory_malloc(size * sizeof(_MQTT_QOS_RECORD))) == NULL) {
		_memory_free(mq);
		return ERR_TR50_MALLOC;
	}
	_memory_memset(mq->records, 0, size * sizeof(_MQTT_QOS_RECORD));

	mq->max_size = size;
	mq->current_size = 0;
	mq->timeout_in_sec = timeout_in_sec;
	_tr50_mutex_create(&mq->mux);
	*mqtt_qos_handle = mq;
	return 0;
}

int mqtt_qos_delete(void *qos) {
	_MQTT_QOS *mq = qos;
	_tr50_mutex_delete(mq->mux);
	_memory_free(mq->records);
	_memory_free(mq);
	return 0;
}

int mqtt_qos_add(void *qos, unsigned short msg_id, mqtt_qos_callback callback, void *custom) {
	_MQTT_QOS *mq = qos;
	int i;

	_tr50_mutex_lock(mq->mux);
	for (i = 0; i < mq->max_size; ++i) {
		if (mq->records[i].is_used) continue;
		mq->records[i].is_used = TRUE;
		mq->records[i].msg_id = msg_id;
		mq->records[i].callback = callback;
		mq->records[i].custom = custom;
		mq->records[i].expiration_timestamp = _time_now_in_sec() + mq->timeout_in_sec;
		++mq->current_size;
		_tr50_mutex_unlock(mq->mux);
		return 0;
	}
	_tr50_mutex_unlock(mq->mux);
	return ERR_MQTT_QOS_FULL;
}

int mqtt_qos_signal(void *qos, unsigned short msg_id) {
	_MQTT_QOS *mq = qos;
	int i;
	mqtt_qos_callback callback;
	void *custom;

	_tr50_mutex_lock(mq->mux);
	if (mq->current_size == 0) {
		_tr50_mutex_unlock(mq->mux);
		return ERR_MQTT_QOS_NOTFOUND;
	}

	for (i = 0; i < mq->max_size; ++i) {
		if (!mq->records[i].is_used) continue;
		if (mq->records[i].msg_id != msg_id) continue;

		mq->records[i].is_used = FALSE;
		callback = mq->records[i].callback;
		custom = mq->records[i].custom;
		--mq->current_size;
		_tr50_mutex_unlock(mq->mux);

		if (callback) callback(0, custom);
		return 0;
	}
	_tr50_mutex_unlock(mq->mux);
	return ERR_MQTT_QOS_NOTFOUND;
}

int mqtt_qos_clear(void *qos, int status) {
	_MQTT_QOS *mq = qos;
	int i;
	mqtt_qos_callback callback;
	void *custom;

	_tr50_mutex_lock(mq->mux);
	if (mq->current_size == 0) {
		_tr50_mutex_unlock(mq->mux);
		return 0;
	}

	for (i = 0; i < mq->max_size; ++i) {
		if (!mq->records[i].is_used) continue;

		mq->records[i].is_used = FALSE;
		callback = mq->records[i].callback;
		custom = mq->records[i].custom;
		--mq->current_size;
		_tr50_mutex_unlock(mq->mux);

		if (callback) callback(status, custom);
		_tr50_mutex_lock(mq->mux);
	}
	_tr50_mutex_unlock(mq->mux);
	return 0;
}

int mqtt_qos_any_expired(void *qos) {
	_MQTT_QOS *mq = qos;
	int i, current_time;

	_tr50_mutex_lock(mq->mux);
	if (mq->current_size == 0) {
		_tr50_mutex_unlock(mq->mux);
		return FALSE;
	}
	current_time = _time_now_in_sec();

	for (i = 0; i < mq->current_size; ++i) {
		if (!mq->records[i].is_used) continue;
		if (mq->records[i].expiration_timestamp - mq->timeout_in_sec > current_time) {
			mq->records[i].expiration_timestamp = mq->timeout_in_sec + current_time;
			continue;
		}
		if (mq->records[i].expiration_timestamp > current_time) continue;
		_tr50_mutex_unlock(mq->mux);
		return TRUE;
	}
	_tr50_mutex_unlock(mq->mux);
	return FALSE;
}
