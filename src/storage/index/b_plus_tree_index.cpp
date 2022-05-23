#include "storage/index/b_plus_tree_index.h"

#include <cstring>
#include <cassert>

namespace thomas {

#define BPLUSTREEINDEX_TYPE BPlusTreeIndex<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREEINDEX_TYPE::BPlusTreeIndex(const std::string &index_name, const KeyComparator &key_comparator, int buffer_pool_size) : key_comparator_(key_comparator) {
  assert(index_name.size() < 32);
  strcpy(index_name_, index_name.c_str());
  disk_manager_ = new DiskManager(index_name + ".db");
  bpm_ = new BufferPoolManager(buffer_pool_size, disk_manager_);

  /* some restore */
  try {
    header_page_ = static_cast<HeaderPage *>(bpm_->FetchPage(HEADER_PAGE_ID));
    page_id_t next_page_id;
    if (!header_page_->SearchRecord("page_amount", &next_page_id)) {
      /* the metadata cannot be broken */
      throw metadata_error();
    }
    disk_manager_->SetNextPageId(next_page_id);
  } catch (read_less_then_a_page &error) {
    /* complicated here, because the page is not fetched successfully */
    bpm_->UnpinPage(HEADER_PAGE_ID, false);
    bpm_->DeletePage(HEADER_PAGE_ID);
    page_id_t header_page_id;
    header_page_ = static_cast<HeaderPage *>(bpm_->NewPage(&header_page_id));
    header_page_->InsertRecord("index", -1);
    header_page_->InsertRecord("page_amount", 1);
  }
  tree_ = new BPLUSTREE_TYPE("index", bpm_, key_comparator_);
}

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREEINDEX_TYPE::~BPlusTreeIndex() {
  header_page_->UpdateRecord("page_amount", disk_manager_->GetNextPageId());
  bpm_->FlushAllPages();
  disk_manager_->ShutDown();
  delete disk_manager_;
  delete bpm_;
  delete tree_;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEX_TYPE::InsertEntry(const KeyType &key, const ValueType &value) {
  tree_->Insert(key, value);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEX_TYPE::DeleteEntry(const KeyType &key) {
  tree_->Remove(key);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEX_TYPE::ScanKey(const KeyType &key, vector<ValueType> *result, const KeyComparator &standby_comparator) {
  tree_->GetValue(key, result, standby_comparator);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEX_TYPE::Debug() {
  tree_->Print(bpm_);
}

template class BPlusTreeIndex<FixedString<48>, size_t, FixedStringComparator<48>>;
template class BPlusTreeIndex<MixedStringInt<68>, int, MixedStringIntComparator<68>>;

}
