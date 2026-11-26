// AgriSense v0.5 - Smart Agriculture CLI built on hand-written data structures.
// Build: g++ -std=c++17 -Wall -Wextra main.cpp -o agrisense && ./agrisense

#include <iostream>
#include <string>
using namespace std;

// Domain models
struct FieldStatus
{
	string id;
	string crop;
	double moisture_pct;
	double temp_c;
	string last_action;

	FieldStatus() : id(""), crop(""), moisture_pct(0.0), temp_c(0.0), last_action("NONE") {}
};

struct ActionItem
{
	string field_id;
	string action;
	int urgency;

	ActionItem() : field_id(""), action(""), urgency(0) {}
};

// 1. Hash table (separate chaining)
class HashTable {
private:
	struct Node {
		string key;
		FieldStatus value;
		Node* next;
	};

	int capacity_;
	int size_;
	Node** buckets_;

	unsigned int hashKey(const string& key) const
	{
		unsigned int h = 0;
		int len = (int)key.length();
		for (int i = 0; i < len; i++)
			h += (unsigned int)(i + 1) * (unsigned int)(unsigned char)key[i];
		return h % (unsigned int)capacity_;
	}

	void clear() {
		for (int i = 0; i < capacity_; i++)
		{
			Node* cur = buckets_[i];
			while (cur != nullptr)
			{
				Node* nxt = cur->next;
				delete cur;
				cur = nxt;
			}
			buckets_[i] = nullptr;
		}
		size_ = 0;
	}

public:
	HashTable(int capacity) : capacity_(capacity > 0 ? capacity : 16), size_(0)
	{
		buckets_ = new Node * [capacity_];
		for (int i = 0; i < capacity_; i++) buckets_[i] = nullptr;
	}

	~HashTable()
	{
		clear();
		delete[] buckets_;
	}

	void insert(const string& key, const FieldStatus& value)
	{
		int idx = (int)hashKey(key);
		for (Node* cur = buckets_[idx]; cur != nullptr; cur = cur->next)
		{
			if (cur->key == key) {
				cur->value = value;  // update in place
				return;
			}
		}
		Node* n = new Node;
		n->key = key;
		n->value = value;
		n->next = buckets_[idx];
		buckets_[idx] = n;
		size_++;
	}

	FieldStatus* get(const string& key)
	{
		int idx = (int)hashKey(key);
		for (Node* cur = buckets_[idx]; cur != nullptr; cur = cur->next)
			if (cur->key == key) return &cur->value;
		return nullptr;
	}

	void printAll() const
	{
		if (size_ == 0)
		{
			cout << "  (no fields stored)\n";
			return;
		}
		for (int i = 0; i < capacity_; i++)
		{
			for (Node* cur = buckets_[i]; cur != nullptr; cur = cur->next)
			{
				const FieldStatus& f = cur->value;
				cout << "  " << f.id << " | " << f.crop << " | moisture=" << f.moisture_pct << "%" << " | temp=" << f.temp_c << "C" << " | action=" << f.last_action << "\n";
			}
		}
	}
	int size() const
	{
		return size_;
	}
};

// ============================================================
// 2. FIFO queue (singly linked list)
// ============================================================
class ActionQueue {
private:
	struct QNode {
		ActionItem item;
		QNode* next;
	};

	QNode* front_;
	QNode* rear_;

public:
	ActionQueue() : front_(nullptr), rear_(nullptr) {}

	~ActionQueue()
	{
		ActionItem tmp;
		while (dequeue(tmp)) {}
	}

	bool isEmpty() const
	{
		return front_ == nullptr;
	}

	void enqueue(const ActionItem& item)
	{
		QNode* n = new QNode;
		n->item = item;
		n->next = nullptr;
		if (rear_ == nullptr)
			front_ = rear_ = n;
		else
		{
			rear_->next = n;
			rear_ = n;
		}
	}

	bool dequeue(ActionItem& out_item)
	{
		if (front_ == nullptr)
			return false;
		QNode* old = front_;
		out_item = old->item;
		front_ = front_->next;
		if (front_ == nullptr)
			rear_ = nullptr;
		delete old;
		return true;
	}
};

// ============================================================
// 3. Max heap keyed on urgency
// ============================================================
class UrgencyHeap {
private:
	ActionItem* data_;
	int capacity_;
	int size_;

	void resize(int new_capacity)
	{
		ActionItem* bigger = new ActionItem[new_capacity];
		for (int i = 0; i < size_; i++)
			bigger[i] = data_[i];
		delete[] data_;
		data_ = bigger;
		capacity_ = new_capacity;
	}

	void swapItems(int a, int b)
	{
		ActionItem tmp = data_[a];
		data_[a] = data_[b];
		data_[b] = tmp;
	}

	void siftUp(int index)
	{
		while (index > 0) {
			int parent = (index - 1) / 2;
			if (data_[index].urgency > data_[parent].urgency)
			{
				swapItems(index, parent);
				index = parent;
			}
			else
				break;
		}
	}

	void siftDown(int index)
	{
		while (true)
		{
			int left = 2 * index + 1;
			int right = 2 * index + 2;
			int largest = index;
			if (left < size_ && data_[left].urgency > data_[largest].urgency)
				largest = left;
			if (right < size_ && data_[right].urgency > data_[largest].urgency)
				largest = right;
			if (largest == index) break;
			swapItems(index, largest);
			index = largest;
		}
	}

public:
	UrgencyHeap(int initial_capacity) : data_(nullptr), capacity_(initial_capacity > 0 ? initial_capacity : 8), size_(0)
	{
		data_ = new ActionItem[capacity_];
	}

	~UrgencyHeap()
	{
		delete[] data_;
	}

	bool isEmpty() const
	{
		return size_ == 0;
	}

	void push(const ActionItem& item)
	{
		if (size_ == capacity_) resize(capacity_ * 2);
		data_[size_] = item;
		siftUp(size_);
		size_++;
	}

	bool popMax(ActionItem& out_item)
	{
		if (size_ == 0) return false;
		out_item = data_[0];
		size_--;
		if (size_ > 0)
		{
			data_[0] = data_[size_];
			siftDown(0);
		}
		return true;
	}
};

// ============================================================
// 4. Binary search tree on moisture (left < node, right >= node)
// ============================================================
class MoistureBST {
private:
	struct BSTNode
	{
		string field_id;
		string crop;
		double moisture_pct;
		BSTNode* left;
		BSTNode* right;
	};

	BSTNode* root_;

	BSTNode* insertNode(BSTNode* node, const string& id, const string& crop, double m)
	{
		if (node == nullptr)
		{
			BSTNode* n = new BSTNode;
			n->field_id = id;
			n->crop = crop;
			n->moisture_pct = m;
			n->left = nullptr;
			n->right = nullptr;
			return n;
		}
		if (m < node->moisture_pct)
			node->left = insertNode(node->left, id, crop, m);
		else
			node->right = insertNode(node->right, id, crop, m);
		return node;
	}

	void printInOrder(const BSTNode* node) const
	{
		if (node == nullptr) return;
		printInOrder(node->left);
		cout << "  " << node->field_id << " (" << node->crop << ") - " << node->moisture_pct << "%\n";
		printInOrder(node->right);
	}

	void printBelowThreshold(const BSTNode* node, double limit, bool& found) const
	{
		if (node == nullptr) return;
		printBelowThreshold(node->left, limit, found);
		if (node->moisture_pct < limit)
		{
			cout << "  " << node->field_id << " (" << node->crop << ") - " << node->moisture_pct << "%\n";
			found = true;
			printBelowThreshold(node->right, limit, found);  // right side may still qualify
		}
	}

	void destroy(BSTNode* node)
	{
		if (node == nullptr) return;
		destroy(node->left);
		destroy(node->right);
		delete node;
	}

public:
	MoistureBST() : root_(nullptr) {}
	~MoistureBST()
	{
		destroy(root_);
	}

	void insert(const string& id, const string& crop, double moisture)
	{
		root_ = insertNode(root_, id, crop, moisture);
	}

	void printSortedByMoisture() const
	{
		if (root_ == nullptr)
		{
			cout << "  (empty)\n";
			return;
		}
		printInOrder(root_);
	}

	void printDryFieldsBelow(double max_moisture) const
	{
		bool found = false;
		printBelowThreshold(root_, max_moisture, found);
		if (!found) cout << "  (no fields below " << max_moisture << "%)\n";
	}
};

// 5. AVL tree on moisture
class MoistureAVL
{
private:
	struct AVLNode
	{
		string field_id;
		string crop;
		double moisture_pct;
		int height;
		AVLNode* left;
		AVLNode* right;
	};

	AVLNode* root_;

	static int maxInt(int a, int b)
	{
		return a > b ? a : b;
	}

	int getHeight(const AVLNode* n) const
	{
		return n == nullptr ? 0 : n->height;
	}

	int getBalance(const AVLNode* n) const
	{
		return n == nullptr ? 0 : getHeight(n->left) - getHeight(n->right);
	}

	void updateHeight(AVLNode* n)
	{
		n->height = maxInt(getHeight(n->left), getHeight(n->right)) + 1;
	}

	AVLNode* rotateRight(AVLNode* y)
	{
		AVLNode* x = y->left;
		AVLNode* moved = x->right;
		x->right = y;
		y->left = moved;
		updateHeight(y);
		updateHeight(x);
		return x;
	}

	AVLNode* rotateLeft(AVLNode* x)
	{
		AVLNode* y = x->right;
		AVLNode* moved = y->left;
		y->left = x;
		x->right = moved;
		updateHeight(x);
		updateHeight(y);
		return y;
	}

	AVLNode* insertNode(AVLNode* node, const string& id, const string& crop, double m) {
		if (node == nullptr)
		{
			AVLNode* n = new AVLNode;
			n->field_id = id;
			n->crop = crop;
			n->moisture_pct = m;
			n->height = 1;
			n->left = nullptr;
			n->right = nullptr;
			return n;
		}
		if (m < node->moisture_pct)
			node->left = insertNode(node->left, id, crop, m);
		else
			node->right = insertNode(node->right, id, crop, m);
		updateHeight(node);
		int balance = getBalance(node);
		// Left-Left
		if (balance > 1 && m < node->left->moisture_pct)
			return rotateRight(node);
		// Right-Right
		if (balance < -1 && m >= node->right->moisture_pct)
			return rotateLeft(node);
		// Left-Right
		if (balance > 1 && m >= node->left->moisture_pct)
		{
			node->left = rotateLeft(node->left);
			return rotateRight(node);
		}
		// Right-Left
		if (balance < -1 && m < node->right->moisture_pct)
		{
			node->right = rotateRight(node->right);
			return rotateLeft(node);
		}
		return node;
	}

	void printInOrder(const AVLNode* node) const
	{
		if (node == nullptr) return;
		printInOrder(node->left);
		cout << "  " << node->field_id << " (" << node->crop << ") - " << node->moisture_pct << "%\n";
		printInOrder(node->right);
	}

	void destroy(AVLNode* node)
	{
		if (node == nullptr) return;
		destroy(node->left);
		destroy(node->right);
		delete node;
	}

public:
	MoistureAVL() : root_(nullptr) {}
	~MoistureAVL()
	{
		destroy(root_);
	}

	void insert(const string& id, const string& crop, double moisture)
	{
		root_ = insertNode(root_, id, crop, moisture);
	}

	void printSortedByMoisture() const
	{
		if (root_ == nullptr) {
			cout << "  (empty)\n";
			return;
		}
		printInOrder(root_);
	}

	int treeHeight() const
	{
		return getHeight(root_);
	}
};

// 6. Farm graph (adjacency lists) + BFS
class FarmGraph {
private:
	struct EdgeNode {
		int to;
		EdgeNode* next;
	};

	int num_nodes_;
	string* labels_;
	EdgeNode** adj_;

	int findIndex(const string& label) const
	{
		for (int i = 0; i < num_nodes_; i++)
			if (labels_[i] == label)
				return i;
		return -1;
	}

	void addEdge(int from, int to)
	{
		EdgeNode* e = new EdgeNode;
		e->to = to;
		e->next = adj_[from];
		adj_[from] = e;
	}

public:
	FarmGraph(int num_nodes) : num_nodes_(num_nodes > 0 ? num_nodes : 1)
	{
		labels_ = new string[num_nodes_];
		adj_ = new EdgeNode * [num_nodes_];
		for (int i = 0; i < num_nodes_; i++)
		{
			labels_[i] = "";
			adj_[i] = nullptr;
		}
	}

	~FarmGraph() {
		for (int i = 0; i < num_nodes_; i++)
		{
			EdgeNode* cur = adj_[i];
			while (cur != nullptr)
			{
				EdgeNode* nxt = cur->next;
				delete cur;
				cur = nxt;
			}
		}
		delete[] adj_;
		delete[] labels_;
	}

	void setLabel(int index, const string& label)
	{
		if (index >= 0 && index < num_nodes_) labels_[index] = label;
	}

	bool connect(const string& from_label, const string& to_label, bool bidirectional)
	{
		int a = findIndex(from_label);
		int b = findIndex(to_label);
		if (a < 0 || b < 0)
			return false;
		addEdge(a, b);
		if (bidirectional)
			addEdge(b, a);
		return true;
	}

	void printConnections() const
	{
		for (int i = 0; i < num_nodes_; i++)
		{
			cout << "  " << labels_[i] << " -> ";
			if (adj_[i] == nullptr)
				cout << "(none)";
			else {
				for (EdgeNode* e = adj_[i]; e != nullptr; e = e->next)
				{
					cout << labels_[e->to];
					if (e->next != nullptr)
						cout << ", ";
				}
			}
			cout << "\n";
		}
	}

	void printReachableFrom(const string& start_label) const
	{
		int start = findIndex(start_label);
		if (start < 0)
		{
			cout << "  Unknown node: " << start_label << "\n";
			return;
		}
		bool* visited = new bool[num_nodes_];
		int* bfs_queue = new int[num_nodes_];
		for (int i = 0; i < num_nodes_; i++) visited[i] = false;
		int head = 0, tail = 0;
		bfs_queue[tail++] = start;
		visited[start] = true;
		cout << "  BFS from " << start_label << ": ";
		bool first = true;
		while (head < tail)
		{
			int cur = bfs_queue[head++];
			if (!first) cout << " -> ";
			cout << labels_[cur];
			first = false;
			for (EdgeNode* e = adj_[cur]; e != nullptr; e = e->next)
			{
				if (!visited[e->to])
				{
					visited[e->to] = true;
					bfs_queue[tail++] = e->to;
				}
			}
		}
		cout << endl;
		delete[] visited;
		delete[] bfs_queue;
	}
};

// Helpers: input recovery, rules engine, recording
void clearBadInput()
{
	cin.clear();
	while (cin && cin.get() != '\n') {}
}
string evaluateField(const FieldStatus& field, int& out_urgency)
{
	if (field.moisture_pct < 30.0)
	{
		out_urgency = (int)(30.0 - field.moisture_pct) + 50;
		return "IRRIGATE_IMMEDIATELY";
	}
	if (field.temp_c > 36.0 && field.moisture_pct < 45.0)
	{
		out_urgency = (int)(field.temp_c - 30.0) + 30;
		return "LIGHT_IRRIGATION_AND_SHADE_CHECK";
	}
	if (field.moisture_pct > 80.0)
	{
		out_urgency = (int)(field.moisture_pct - 80.0) + 20;
		return "PAUSE_IRRIGATION_DRAINAGE_CHECK";
	}
	out_urgency = 0;
	return "MONITOR_NORMAL";
}

void recordFieldReading(const string& id, const string& crop, double moisture, double temp, HashTable& table, ActionQueue& queue, UrgencyHeap& heap, MoistureBST& bst, MoistureAVL& avl) {
	FieldStatus f;
	f.id = id;
	f.crop = crop;
	f.moisture_pct = moisture;
	f.temp_c = temp;
	int urgency = 0;
	f.last_action = evaluateField(f, urgency);
	table.insert(f.id, f);
	bst.insert(f.id, f.crop, f.moisture_pct);
	avl.insert(f.id, f.crop, f.moisture_pct);
	bool queued = false;
	if (f.last_action != "MONITOR_NORMAL")
	{
		ActionItem item;
		item.field_id = f.id;
		item.action = f.last_action;
		item.urgency = urgency;
		queue.enqueue(item);
		heap.push(item);
		queued = true;
	}
	cout << "[" << f.id << "] " << f.crop << " " << f.moisture_pct << "% / " << f.temp_c << "C -> " << f.last_action << " (u=" << urgency << ")" << (queued ? " [queued]" : "") << "\n";
}

void printMenu()
{
	cout << "\n===== AgriSense v0.5 =====\n" << "1. Add / update field sensor reading\n" << "2. Lookup field by ID\n" << "3. View all fields\n" << "4. View fields sorted by moisture (BST & AVL)\n" << "5. Query dry fields below moisture threshold\n" << "6. Dispatch next action by urgency (heap)\n" << "7. Dispatch next action in arrival order (queue)\n" << "8. View farm connections & BFS reachability\n" << "0. Exit\n" << "Choice: ";
}

// main
int main() {
	HashTable table(16);
	ActionQueue queue;
	UrgencyHeap heap(8);
	MoistureBST bst;
	MoistureAVL avl;

	cout << "Loading sample fields...\n";
	recordFieldReading("F-101", "Wheat", 24.5, 31.0, table, queue, heap, bst, avl);
	recordFieldReading("F-102", "Corn", 52.0, 29.5, table, queue, heap, bst, avl);
	recordFieldReading("F-103", "Rice", 84.0, 30.0, table, queue, heap, bst, avl);
	recordFieldReading("F-104", "Cotton", 38.0, 38.5, table, queue, heap, bst, avl);
	recordFieldReading("F-105", "Barley", 19.0, 33.0, table, queue, heap, bst, avl);

	FarmGraph farm_graph(6);
	farm_graph.setLabel(0, "Pump-1");
	farm_graph.setLabel(1, "F-101");
	farm_graph.setLabel(2, "F-102");
	farm_graph.setLabel(3, "F-103");
	farm_graph.setLabel(4, "F-104");
	farm_graph.setLabel(5, "F-105");
	farm_graph.connect("Pump-1", "F-101", true);
	farm_graph.connect("Pump-1", "F-102", true);
	farm_graph.connect("F-102", "F-103", true);
	farm_graph.connect("F-103", "F-104", true);
	farm_graph.connect("F-104", "F-105", true);

	int choice = -1;
	while (choice != 0)
	{
		printMenu();
		if (!(cin >> choice))
		{
			if (cin.eof()) break;
			clearBadInput();
			cout << "Invalid input. Please enter a number.\n";
			choice = -1;
			continue;
		}
		if (choice == 1)
		{
			string id, crop;
			double moisture, temp;
			cout << "Field ID (no spaces): ";
			cin >> id;
			cout << "Crop (no spaces): ";
			cin >> crop;
			cout << "Moisture %: ";
			cin >> moisture;
			cout << "Temperature C: ";
			cin >> temp;
			if (cin.fail())
			{
				clearBadInput();
				cout << "Invalid numeric input. Reading discarded.\n";
			}
			else
				recordFieldReading(id, crop, moisture, temp, table, queue, heap, bst, avl);
		}
		else if (choice == 2)
		{
			string id;
			cout << "Field ID: ";
			cin >> id;
			FieldStatus* f = table.get(id);
			if (f == nullptr)
				cout << "  Field not found.\n";
			else
				cout << "  " << f->id << " | " << f->crop << " | moisture=" << f->moisture_pct << "% | temp=" << f->temp_c << "C | action=" << f->last_action << "\n";
		}
		else if (choice == 3)
		{
			cout << "All fields (" << table.size() << "):\n";
			table.printAll();
		}
		else if (choice == 4)
		{
			cout << "BST (in-order):\n";
			bst.printSortedByMoisture();
			cout << "AVL (in-order):\n";
			avl.printSortedByMoisture();
			cout << "AVL height: " << avl.treeHeight() << "\n";
		}
		else if (choice == 5)
		{
			double limit;
			cout << "Moisture threshold %: ";
			cin >> limit;
			if (cin.fail())
			{
				clearBadInput();
				cout << "Invalid threshold.\n";
			}
			else
			{
				cout << "Fields below " << limit << "%:\n";
				bst.printDryFieldsBelow(limit);
			}
		}
		else if (choice == 6)
		{
			ActionItem item;
			if (heap.popMax(item))
				cout << "  Most urgent: " << item.field_id << " -> " << item.action << " (u=" << item.urgency << ")\n";
			else
				cout << "  No pending actions.\n";
		}
		else if (choice == 7)
		{
			ActionItem item;
			if (queue.dequeue(item))
				cout << "  Next in line: " << item.field_id << " -> " << item.action << " (u=" << item.urgency << ")\n";
			else
				cout << "  No pending actions.\n";
		}
		else if (choice == 8)
		{
			cout << "Farm connections:\n";
			farm_graph.printConnections();
			string start;
			cout << "BFS start node (e.g. Pump-1): ";
			cin >> start;
			farm_graph.printReachableFrom(start);
		}
		else if (choice == 0)
			cout << "Goodbye.\n";
		else
			cout << "Unknown option.\n";
	}
}