#include "data.h"

PullPolicy* newPullPolicy() {
	PullPolicy* ret = (PullPolicy*) malloc(sizeof(PullPolicy));
	ret->rtAddressBund = 0;
	ret->rtReqInvokeId = 0;
	ret->next = NULL;
	ret->propNum = 0;
	return ret;
}

BacProperty* newBacProperty() {
	BacProperty* ret = (BacProperty*) malloc(sizeof(BacProperty));
	ret->index = -1;
	return ret;
}