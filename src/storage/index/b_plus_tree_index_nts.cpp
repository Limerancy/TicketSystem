#include "storage/index/b_plus_tree_index_nts.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>

#include "common/config.h"
#include "concurrency/transaction.h"

namespace thomas {

#define BPLUSTREEINDEXNTS_TYPE BPlusTreeIndexNTS<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREEINDEXNTS_TYPE::BPlusTreeIndexNTS(const std::string &index_name, const KeyComparator &key_comparator,
                                          int buffer_pool_size)
    : key_comparator_(key_comparator) {
  assert(index_name.size() < 32);
  strcpy(index_name_, index_name.c_str());
  disk_manager_ = new DiskManager(index_name + ".db");
  bpm_ = new BufferPoolManager(buffer_pool_size, disk_manager_, THREAD_SAFE_TYPE::NON_THREAD_SAFE);

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
  tree_ = new BPLUSTREENTS_TYPE("index", bpm_, key_comparator_);
}

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREEINDEXNTS_TYPE::~BPlusTreeIndexNTS() {
  header_page_->UpdateRecord("page_amount", disk_manager_->GetNextPageId());
  bpm_->UnpinPage(HEADER_PAGE_ID, true);
  bpm_->FlushAllPages();
  disk_manager_->ShutDown();
  delete disk_manager_;
  delete bpm_;
  delete tree_;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEXNTS_TYPE::Debug() { tree_->Print(bpm_); }

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEXNTS_TYPE::InsertEntry(const KeyType &key, const ValueType &value, std::mutex *mutex) {
  Transaction *transaction = new Transaction(mutex);
  tree_->Insert(key, value, transaction);
  delete transaction;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEXNTS_TYPE::DeleteEntry(const KeyType &key, std::mutex *mutex) {
  Transaction *transaction = new Transaction(mutex);
  tree_->Remove(key, transaction);
  delete transaction;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEXNTS_TYPE::ScanKey(const KeyType &key, vector<ValueType> *result,
                                     const KeyComparator &standby_comparator, std::mutex *mutex) {
  Transaction *transaction = new Transaction(mutex);
  tree_->GetValue(key, result, standby_comparator, transaction);
  delete transaction;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREEINDEXNTS_TYPE::SearchKey(const KeyType &key, vector<ValueType> *result, std::mutex *mutex) {
  Transaction *transaction = new Transaction(mutex);
  tree_->GetValue(key, result, transaction);
  delete transaction;
}

template class BPlusTreeIndexNTS<FixedString<48>, size_t, FixedStringComparator<48>>;
template class BPlusTreeIndexNTS<MixedStringInt<68>, int, MixedStringIntComparator<68>>;

}  // namespace thomas