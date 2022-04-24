#pragma once

class SetMember : public object {
public:
	virtual void print(void const *) const;
	virtual boolean select(void const *) const;

	struct select_args {
		void const *condition;
		void const *print_arg;
		size_t count;
	};

	static void selectTrampoline(ref<set_member> mbr, void const *arg);
	static void printTrampoline(ref<set_member> mbr, void const *arg);

	SetMember(class_descriptor &desc) : object(desc) {}
	SetMember(class_descriptor &desc, size_t varyingSize)
		: object(desc, varyingSize) {}

	METACLASS_DECLARATIONS(SetMember, object);
};
