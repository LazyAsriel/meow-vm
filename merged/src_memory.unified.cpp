// =============================================================================
//  FILE PATH: src/memory/generational_gc.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "memory/generational_gc.h"
     3	#include <meow/value.h>
     4	#include "runtime/execution_context.h"
     5	#include <meow/core/meow_object.h>
     6	#include <module/module_manager.h>
     7	#include "meow_heap.h"
     8	
     9	namespace meow {
    10	
    11	using namespace gc_flags;
    12	
    13	static void clear_list(heap* h, ObjectMeta* head) {
    14	    while (head) {
    15	        ObjectMeta* next = head->next_gc;
    16	        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(head));
    17	        std::destroy_at(obj);
    18	        h->deallocate_raw(head, sizeof(ObjectMeta) + head->size);
    19	        head = next;
    20	    }
    21	}
    22	
    23	GenerationalGC::~GenerationalGC() noexcept {
    24	    if (heap_) {
    25	        clear_list(heap_, young_head_);
    26	        clear_list(heap_, old_head_);
    27	        clear_list(heap_, perm_head_);
    28	    }
    29	}
    30	
    31	void GenerationalGC::register_object(const MeowObject* object) {
    32	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    33	    
    34	    meta->next_gc = young_head_;
    35	    young_head_ = meta;
    36	    
    37	    meta->flags = GEN_YOUNG;
    38	    
    39	    young_count_++;
    40	}
    41	
    42	void GenerationalGC::register_permanent(const MeowObject* object) {
    43	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    44	    
    45	    meta->next_gc = perm_head_;
    46	    perm_head_ = meta;
    47	    
    48	    meta->flags = GEN_OLD | PERMANENT | MARKED;
    49	}
    50	
    51	void GenerationalGC::write_barrier(MeowObject* owner, Value value) noexcept {
    52	    auto* owner_meta = heap::get_meta(owner);
    53	    
    54	    if (!(owner_meta->flags & GEN_OLD)) return;
    55	
    56	    if (value.is_object()) {
    57	        MeowObject* target = value.as_object();
    58	        if (target) {
    59	            auto* target_meta = heap::get_meta(target);
    60	            if (!(target_meta->flags & GEN_OLD)) {
    61	                remembered_set_.push_back(owner);
    62	            }
    63	        }
    64	    }
    65	}
    66	
    67	size_t GenerationalGC::collect() noexcept {
    68	    context_->trace(*this);
    69	    module_manager_->trace(*this);
    70	    
    71	    ObjectMeta* perm = perm_head_;
    72	    while (perm) {
    73	        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(perm));
    74	        if (obj->get_type() != ObjectType::STRING) {
    75	            mark_object(obj);
    76	        }
    77	        perm = perm->next_gc;
    78	    }
    79	
    80	    // [FIX] Quét Remembered Set để đánh dấu các đối tượng con (Young Gen)
    81	    // được tham chiếu bởi đối tượng già (Old Gen).
    82	    for (auto* obj : remembered_set_) {
    83	        mark_object(obj);
    84	    }
    85	
    86	    if (old_count_ > old_gen_threshold_) {
    87	        sweep_full();
    88	        old_gen_threshold_ = std::max((size_t)100, old_count_ * 2);
    89	    } else {
    90	        sweep_young();
    91	    }
    92	
    93	    remembered_set_.clear();
    94	    return young_count_ + old_count_;
    95	}
    96	
    97	void GenerationalGC::destroy_object(ObjectMeta* meta) {
    98	    MeowObject* obj = static_cast<MeowObject*>(heap::get_data(meta));
    99	    std::destroy_at(obj);
   100	    heap_->deallocate_raw(meta, sizeof(ObjectMeta) + meta->size);
   101	}
   102	
   103	void GenerationalGC::sweep_young() {
   104	    ObjectMeta** curr = &young_head_;
   105	    size_t survived = 0;
   106	
   107	    while (*curr) {
   108	        ObjectMeta* meta = *curr;
   109	        
   110	        if (meta->flags & MARKED) {
   111	            *curr = meta->next_gc; 
   112	            meta->next_gc = old_head_;
   113	            old_head_ = meta;
   114	            meta->flags = GEN_OLD; 
   115	            
   116	            old_count_++;
   117	            young_count_--;
   118	        } else {
   119	            ObjectMeta* dead = meta;
   120	            *curr = dead->next_gc;
   121	            
   122	            destroy_object(dead);
   123	            young_count_--;
   124	        }
   125	    }
   126	}
   127	
   128	void GenerationalGC::sweep_full() {
   129	    ObjectMeta** curr_old = &old_head_;
   130	    size_t old_survived = 0;
   131	    while (*curr_old) {
   132	        ObjectMeta* meta = *curr_old;
   133	        if (meta->flags & MARKED) {
   134	            meta->flags &= ~MARKED;
   135	            curr_old = &meta->next_gc;
   136	            old_survived++;
   137	        } else {
   138	            ObjectMeta* dead = meta;
   139	            *curr_old = dead->next_gc;
   140	            destroy_object(dead);
   141	        }
   142	    }
   143	    old_count_ = old_survived;
   144	
   145	    sweep_young(); 
   146	}
   147	
   148	void GenerationalGC::visit_value(param_t value) noexcept {
   149	    if (value.is_object()) mark_object(value.as_object());
   150	}
   151	
   152	void GenerationalGC::visit_object(const MeowObject* object) noexcept {
   153	    mark_object(const_cast<MeowObject*>(object));
   154	}
   155	
   156	void GenerationalGC::mark_object(MeowObject* object) {
   157	    if (object == nullptr) return;
   158	    
   159	    auto* meta = heap::get_meta(object);
   160	    
   161	    if (meta->flags & MARKED) return;
   162	    
   163	    meta->flags |= MARKED;
   164	    
   165	    object->trace(*this);
   166	}
   167	
   168	}


// =============================================================================
//  FILE PATH: src/memory/generational_gc.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/common.h>
     5	#include <meow/memory/garbage_collector.h>
     6	#include <meow/memory/gc_visitor.h>
     7	#include <vector> 
     8	#include "meow_heap.h"
     9	
    10	namespace meow {
    11	struct ExecutionContext;
    12	class ModuleManager;
    13	
    14	class GenerationalGC : public GarbageCollector, public GCVisitor {
    15	public:
    16	    explicit GenerationalGC(ExecutionContext* context) noexcept : context_(context) {}
    17	    ~GenerationalGC() noexcept override;
    18	
    19	    void register_object(const MeowObject* object) override;
    20	    void register_permanent(const MeowObject* object) override;
    21	    size_t collect() noexcept override;
    22	
    23	    void write_barrier(MeowObject* owner, Value value) noexcept override;
    24	
    25	    void visit_value(param_t value) noexcept override;
    26	    void visit_object(const MeowObject* object) noexcept override;
    27	
    28	    void set_module_manager(ModuleManager* mm) { module_manager_ = mm; }
    29	
    30	private:
    31	    ExecutionContext* context_ = nullptr;
    32	    ModuleManager* module_manager_ = nullptr;
    33	
    34	    ObjectMeta* young_head_ = nullptr;
    35	    ObjectMeta* old_head_   = nullptr;
    36	    ObjectMeta* perm_head_  = nullptr;
    37	    
    38	    std::vector<MeowObject*> remembered_set_;
    39	
    40	    size_t young_count_ = 0;
    41	    size_t old_count_ = 0;
    42	    size_t old_gen_threshold_ = 100;
    43	
    44	    void mark_object(MeowObject* object);
    45	    
    46	    void sweep_young(); 
    47	    void sweep_full();
    48	    
    49	    void destroy_object(ObjectMeta* meta);
    50	};
    51	}


// =============================================================================
//  FILE PATH: src/memory/mark_sweep_gc.cpp
// =============================================================================

     1	#include "pch.h"
     2	#include "memory/mark_sweep_gc.h"
     3	#include <meow/value.h>
     4	#include <module/module_manager.h>
     5	#include "runtime/execution_context.h"
     6	#include "meow_heap.h"
     7	
     8	namespace meow {
     9	    
    10	using namespace gc_flags;
    11	
    12	MarkSweepGC::~MarkSweepGC() noexcept {
    13	    while (head_) {
    14	        ObjectMeta* next = head_->next_gc;
    15	        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(head_));
    16	        std::destroy_at(obj);
    17	        if (heap_) heap_->deallocate_raw(head_, sizeof(ObjectMeta) + head_->size);
    18	        head_ = next;
    19	    }
    20	}
    21	
    22	void MarkSweepGC::register_object(const MeowObject* object) {
    23	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    24	    meta->next_gc = head_;
    25	    head_ = meta;
    26	    meta->flags = 0;
    27	    object_count_++;
    28	}
    29	
    30	void MarkSweepGC::register_permanent(const MeowObject* object) {
    31	    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    32	    meta->flags = MARKED | PERMANENT; 
    33	}
    34	
    35	size_t MarkSweepGC::collect() noexcept {
    36	    context_->trace(*this);
    37	    module_manager_->trace(*this);
    38	    ObjectMeta** curr = &head_;
    39	    size_t survived = 0;
    40	
    41	    while (*curr) {
    42	        ObjectMeta* meta = *curr;
    43	        
    44	        if (meta->flags & PERMANENT) {
    45	            curr = &meta->next_gc;
    46	        }
    47	        else if (meta->flags & MARKED) {
    48	            meta->flags &= ~MARKED;
    49	            curr = &meta->next_gc;
    50	            survived++;
    51	        } else {
    52	            ObjectMeta* dead = meta;
    53	            *curr = dead->next_gc;
    54	            
    55	            MeowObject* obj = static_cast<MeowObject*>(heap::get_data(dead));
    56	            std::destroy_at(obj);
    57	            heap_->deallocate_raw(dead, sizeof(ObjectMeta) + dead->size);
    58	            
    59	            object_count_--;
    60	        }
    61	    }
    62	
    63	    return survived;
    64	}
    65	
    66	void MarkSweepGC::visit_value(param_t value) noexcept {
    67	    if (value.is_object()) mark(value.as_object());
    68	}
    69	
    70	void MarkSweepGC::visit_object(const MeowObject* object) noexcept {
    71	    mark(const_cast<MeowObject*>(object));
    72	}
    73	
    74	void MarkSweepGC::mark(MeowObject* object) {
    75	    if (object == nullptr) return;
    76	    auto* meta = heap::get_meta(object);
    77	    
    78	    if (meta->flags & MARKED) return;
    79	    
    80	    meta->flags |= MARKED;
    81	    object->trace(*this);
    82	}
    83	
    84	}


// =============================================================================
//  FILE PATH: src/memory/mark_sweep_gc.h
// =============================================================================

     1	#pragma once
     2	
     3	#include "pch.h"
     4	#include <meow/common.h>
     5	#include <meow/memory/garbage_collector.h>
     6	#include <meow/memory/gc_visitor.h>
     7	#include "meow_heap.h"
     8	
     9	namespace meow {
    10	struct ExecutionContext;
    11	class ModuleManager;
    12	
    13	class MarkSweepGC : public GarbageCollector, public GCVisitor {
    14	public:
    15	    explicit MarkSweepGC(ExecutionContext* context) noexcept : context_(context) {}
    16	    ~MarkSweepGC() noexcept override;
    17	
    18	    void register_object(const MeowObject* object) override;
    19	    void register_permanent(const MeowObject* object) override;
    20	    size_t collect() noexcept override;
    21	
    22	    void visit_value(param_t value) noexcept override;
    23	    void visit_object(const MeowObject* object) noexcept override;
    24	
    25	    void set_module_manager(ModuleManager* mm) { module_manager_ = mm; }
    26	private:
    27	    ExecutionContext* context_ = nullptr;
    28	    ModuleManager* module_manager_ = nullptr;
    29	    
    30	    ObjectMeta* head_ = nullptr;
    31	    size_t object_count_ = 0;
    32	
    33	    void mark(MeowObject* object);
    34	};
    35	}


// =============================================================================
//  FILE PATH: src/memory/memory_manager.cpp
// =============================================================================

     1	#include <meow/memory/memory_manager.h>
     2	#include <meow/core/objects.h>
     3	
     4	namespace meow {
     5	
     6	MemoryManager::MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept 
     7	    : arena_(64 * 1024), 
     8	      heap_(arena_),
     9	      gc_(std::move(gc)), 
    10	      gc_threshold_(1e7), 
    11	      object_allocated_(0) 
    12	{ 
    13	    if (gc_) {
    14	        gc_->set_heap(&heap_);
    15	    }
    16	}
    17	MemoryManager::~MemoryManager() noexcept {}
    18	
    19	string_t MemoryManager::new_string(std::string_view str_view) {
    20	    if (auto it = string_pool_.find(str_view); it != string_pool_.end()) {
    21	        return *it;
    22	    }
    23	    
    24	    size_t length = str_view.size();
    25	    size_t hash = std::hash<std::string_view>{}(str_view);
    26	    
    27	    string_t new_obj = heap_.create_varsize<ObjString>(length, str_view.data(), length, hash);
    28	    
    29	    gc_->register_permanent(new_obj);
    30	    
    31	    object_allocated_++;
    32	    string_pool_.insert(new_obj);
    33	    return new_obj;
    34	}
    35	
    36	string_t MemoryManager::new_string(const char* chars, size_t length) {
    37	    return new_string(std::string(chars, length));
    38	}
    39	
    40	array_t MemoryManager::new_array(const std::vector<Value>& elements) {
    41	    // meow::allocator<Value> alloc(arena_);
    42	    
    43	    // return new_object<ObjArray>(elements, alloc);
    44	    return new_object<ObjArray>(elements);
    45	}
    46	
    47	hash_table_t MemoryManager::new_hash(uint32_t capacity) {
    48	    auto alloc = heap_.get_allocator<Entry>();
    49	    return heap_.create<ObjHashTable>(alloc, capacity);
    50	}
    51	
    52	upvalue_t MemoryManager::new_upvalue(size_t index) {
    53	    return new_object<ObjUpvalue>(index);
    54	}
    55	
    56	proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk) {
    57	    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk));
    58	}
    59	
    60	proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs) {
    61	    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk), std::move(descs));
    62	}
    63	
    64	function_t MemoryManager::new_function(proto_t proto) {
    65	    return new_object<ObjClosure>(proto);
    66	}
    67	
    68	module_t MemoryManager::new_module(string_t file_name, string_t file_path, proto_t main_proto) {
    69	    return new_object<ObjModule>(file_name, file_path, main_proto);
    70	}
    71	
    72	class_t MemoryManager::new_class(string_t name) {
    73	    return new_object<ObjClass>(name);
    74	}
    75	
    76	instance_t MemoryManager::new_instance(class_t klass, Shape* shape) {
    77	    return new_object<ObjInstance>(klass, shape);
    78	}
    79	
    80	bound_method_t MemoryManager::new_bound_method(Value instance, Value function) {
    81	    return new_object<ObjBoundMethod>(instance, function);
    82	}
    83	
    84	Shape* MemoryManager::new_shape() {
    85	    return new_object<Shape>();
    86	}
    87	
    88	Shape* MemoryManager::get_empty_shape() noexcept {
    89	    if (empty_shape_ == nullptr) {
    90	        empty_shape_ = heap_.create<Shape>(); 
    91	        
    92	        if (gc_) gc_->register_permanent(empty_shape_);
    93	        
    94	        object_allocated_++;
    95	    }
    96	    return empty_shape_;
    97	}
    98	
    99	}


