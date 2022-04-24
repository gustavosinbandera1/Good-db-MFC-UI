#pragma once

template<class T>
class small_set {
public:
	typedef void (T::*const_iterator)(void const* arg) const;
	typedef void (T::*iterator)(void const* arg);

	ref<set_owner> members;

protected:
	struct invocation {
		iterator    func;
		void const* arg;
	};
	struct const_invocation {
		const_iterator func;
		void const*    arg;
	};

	static void const_trampoline(ref<set_member> mbr, void const* arg) {
		ref<T> obj = mbr->obj;
		const_invocation* ip = (const_invocation*)arg;
		(((obj.operator->()).operator->())->*ip->func)(ip->arg);
	}

	static void trampoline(ref<set_member> mbr, void const* arg) {
		ref<T> obj = mbr->obj;
		invocation* ip = (invocation*)arg;
		((modify(obj).operator->())->*ip->func)(ip->arg);
	}

public:
	size_t size() const { return members->n_members; }
	boolean empty() const { return members->empty(); }

	void apply(const_iterator iter, void const* arg = NULL) const {
		const_invocation inv;
		inv.arg = arg;
		inv.func = iter;
		members->iterate(const_trampoline, &inv);
	}

	void apply(iterator iter, void const* arg = NULL) const {
		invocation inv;
		inv.arg = arg;
		inv.func = iter;
		members->iterate(trampoline, &inv);
	}

	size_t print(void const* arg = NULL) const {
		return members->iterate(SetMember::printTrampoline, arg);
	}

	size_t select(void const* condition = NULL,
		void const* print_arg = NULL) const
	{
		typename SetMember::select_args args;
		args.condition = condition;
		args.print_arg = print_arg;
		args.count = 0;
		members->iterate(SetMember::selectTrampoline, &args);
		return args.count;
	}

	ref<T> find(char const* key) const {
		ref<set_member> mbr = members->find(key);
		if (mbr == NULL) {
			return NULL;
		}
		return mbr->obj;
	}

	ref<T> erase(char const* key) const {
		ref<set_member> mbr = members->find(key);
		if (mbr == NULL) {
			return NULL;
		}
		modify(members)->remove(mbr);
		return mbr->obj;
	}

	void insert(char const* key, ref<T> obj) const {
		modify(members)->insert(key, obj);
	}

	boolean insertUnique(char const* key, ref<T> obj) const {
		return modify(members)->insert_unique(key, obj);
	}

	field_descriptor& describe_components() const { return FIELD(members); }

	virtual ~small_set() {}

	virtual void initialize(object* owner) {
		members = set_owner::create(owner);
	}
};

template<class T>
inline field_descriptor& describe_field(small_set<T> const& s) {
	return s.describe_components();
}

template<class T>
class large_set : public small_set<T> {
public:
	virtual void initialize(object* owner) {
		small_set<T>::members = B_tree::create(owner);
	}
};
