// =============================================================================
//  FILE PATH: src/core/objects.cpp
// =============================================================================

     1	#include <meow/core/objects.h>
     2	#include <meow/memory/gc_visitor.h>
     3	#include <meow/memory/memory_manager.h>
     4	
     5	namespace meow {
     6	
     7	void ObjArray::trace(GCVisitor& visitor) const noexcept {
     8	    for (const auto& element : elements_) {
     9	        visitor.visit_value(element);
    10	    }
    11	}
    12	
    13	void ObjClass::trace(GCVisitor& visitor) const noexcept {
    14	    visitor.visit_object(name_);
    15	    visitor.visit_object(superclass_);
    16	    
    17	    const auto& keys = methods_.keys();
    18	    const auto& vals = methods_.values();
    19	    const size_t size = keys.size();
    20	    for (size_t i = 0; i < size; ++i) {
    21	        visitor.visit_object(keys[i]);
    22	        visitor.visit_value(vals[i]);
    23	    }
    24	}
    25	
    26	void ObjUpvalue::trace(GCVisitor& visitor) const noexcept {
    27	    visitor.visit_value(closed_);
    28	}
    29	
    30	void ObjFunctionProto::trace(GCVisitor& visitor) const noexcept {
    31	    visitor.visit_object(name_);
    32	    visitor.visit_object(module_);
    33	    for (size_t i = 0; i < chunk_.get_pool_size(); ++i) {
    34	        visitor.visit_value(chunk_.get_constant(i));
    35	    }
    36	}
    37	
    38	void ObjClosure::trace(GCVisitor& visitor) const noexcept {
    39	    visitor.visit_object(proto_);
    40	    for (const auto& upvalue : upvalues_) {
    41	        visitor.visit_object(upvalue);
    42	    }
    43	}
    44	
    45	void ObjModule::trace(GCVisitor& visitor) const noexcept {
    46	    visitor.visit_object(file_name_);
    47	    visitor.visit_object(file_path_);
    48	    visitor.visit_object(main_proto_);
    49	
    50	    for (const auto& val : globals_store_) {
    51	        visitor.visit_value(val);
    52	    }
    53	    
    54	    const auto& g_keys = global_names_.keys();
    55	    for (auto key : g_keys) {
    56	        visitor.visit_object(key);
    57	    }
    58	
    59	    const auto& e_keys = exports_.keys();
    60	    const auto& e_vals = exports_.values();
    61	    
    62	    const size_t e_size = e_keys.size();
    63	    for (size_t i = 0; i < e_size; ++i) {
    64	        visitor.visit_object(e_keys[i]);
    65	        visitor.visit_value(e_vals[i]);
    66	    }
    67	}
    68	
    69	}


// =============================================================================
//  FILE PATH: src/core/shape.cpp
// =============================================================================

     1	#include <meow/core/shape.h>
     2	#include <meow/memory/memory_manager.h>
     3	
     4	namespace meow {
     5	
     6	int Shape::get_offset(string_t name) const {
     7	    if (const uint32_t* ptr = property_offsets_.find(name)) {
     8	        return static_cast<int>(*ptr);
     9	    }
    10	    return -1;
    11	}
    12	
    13	Shape* Shape::get_transition(string_t name) const {
    14	    if (Shape* const* ptr = transitions_.find(name)) {
    15	        return *ptr;
    16	    }
    17	    return nullptr;
    18	}
    19	
    20	Shape* Shape::add_transition(string_t name, MemoryManager* heap) {
    21	    heap->disable_gc();
    22	    Shape* new_shape = heap->new_shape();
    23	    
    24	    new_shape->copy_from(this);
    25	    new_shape->add_property(name);
    26	
    27	    transitions_.try_emplace(name, new_shape);
    28	    
    29	    heap->write_barrier(this, Value(reinterpret_cast<object_t>(new_shape))); 
    30	    heap->enable_gc();
    31	    return new_shape;
    32	}
    33	
    34	void Shape::trace(GCVisitor& visitor) const noexcept {
    35	    const auto& prop_keys = property_offsets_.keys();
    36	    for (auto key : prop_keys) {
    37	        visitor.visit_object(key);
    38	    }
    39	
    40	    const auto& trans_keys = transitions_.keys();
    41	    const auto& trans_vals = transitions_.values();
    42	    
    43	    for (size_t i = 0; i < trans_keys.size(); ++i) {
    44	        visitor.visit_object(trans_keys[i]);
    45	        visitor.visit_object(trans_vals[i]);
    46	    }
    47	}
    48	
    49	}


