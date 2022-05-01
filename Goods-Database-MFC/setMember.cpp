#include "pch.h"
#include "setMember.h"
#include <vector>

void SetMember::print(void const *) const { console::output("testing here.."); }


boolean SetMember::select(void const *) const { return False; }


void SetMember::selectTrampoline(ref<set_member> mbr, void const *arg) {
	ref<SetMember> obj = mbr->obj;
	select_args *args = (select_args *)arg;
	if (obj->select(args->condition)) {
		args->count += 1;
		obj->print(args->print_arg);
	}
}

void SetMember::printTrampoline(ref<set_member> mbr, void const *arg) {
	if (mbr->prev == NULL) {
		console::output("\n\n********** INITIALIZING PRINTER ************");
	}
	console::output("\n..............................\n");
	ref<SetMember> obj = mbr->obj;
	obj->print(arg);
	if (mbr->next == NULL) {
		console::output("\n\n********** FINISHING PRINTER ***************\n");
	}
}

field_descriptor &SetMember::describe_components() { return NO_FIELDS; }

REGISTER(SetMember, object, pessimistic_scheme);
