<?php

@include_once 'sites/all/libraries/tuque/Datastream.php';
@include_once 'sites/all/libraries/tuque/FedoraApi.php';
@include_once 'sites/all/libraries/tuque/FedoraApiSerializer.php';
@include_once 'sites/all/libraries/tuque/Object.php';
@include_once 'sites/all/libraries/tuque/RepositoryConnection.php';
@include_once 'sites/all/libraries/tuque/Cache.php';
@include_once 'sites/all/libraries/tuque/RepositoryException.php';
@include_once 'sites/all/libraries/tuque/Repository.php';
@include_once 'sites/all/libraries/tuque/FedoraRelationships.php';

function migrate_books_by_collection_form(array $form, array &$form_state) {
  $form = array();
  $form['collection_pid'] = array(
    '#type' => 'textfield',
    '#description' => "Enter pid of collection to be migrated"
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Migrate this collection.'),
  );
  return $form;
}

function migrate_books_by_collection_form_submit(array $form, array &$form_state) {
  $fedora_49_url = "http://modernistcommons.ca:8080/fedora";
  $fedora_50_url = "http://137.149.200.50:8080/fedora";
  $repository_49 = migrate_books_create_repository($fedora_49_url);
  $repository_50 = migrate_books_create_repository($fedora_50_url);
  $collection_pid = $form_state['values']['collection_pid'];
  // Build new collection on .50 to house migrated books.
  $new_collection_pid = migrate_books_build_new_collection($repository_49, $repository_50, $collection_pid);
  $book_pids = islandora_book_migrate_get_book_pids($collection_pid, $repository_49);
  // Move to batch


  $batch = array(
    'title' => t('Migrating Collections'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'migrate_books') . '/includes/book_migrate.inc',
    'progress_message' => t('@current of @total objects created.'),
  );

  foreach ($book_pids as $book_pid) {
    $page_pids = islandora_migrate_get_page_pids($book_pid, $repository_49);
    $batch['operations'][] = array('islandora_migrate_book', array(
        'book_pid' => $book_pid,
        'page_pids' => $page_pids,
        'new_collection_pid' => $new_collection_pid,
        'repository_49' => $repository_49,
        'repository_50' => $repository_50,
    ));
  }

  batch_set($batch);
  batch_process();
}

function islandora_book_migrate_get_book_pids($collection_pid, $repository) {
  $query = <<<QUERY
select \$subject from <#ri> 
    where (
      (\$subject <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> <info:fedora/$collection_pid>
       or
       \$subject <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/$collection_pid>
      )
    and
      (
      \$subject <info:fedora/fedora-system:def/model#hasModel> <info:fedora/islandora:iaBookCModel>
      or
      \$subject <info:fedora/fedora-system:def/model#hasModel> <info:fedora/islandora:bookCModel>
      )
    )
QUERY;
  $book_pids = array();
  $collection_objects = $repository->ri->itqlQuery($query, 'unlimited');
  foreach ($collection_objects as $collection_object) {
    $book_pids[] = $collection_object['subject']['value'];
  }
  return $book_pids;
}

function islandora_migrate_get_page_pids($book_pid, $repository) {
  $query = <<<QUERY
select \$subject from <#ri> 
    where (
      (\$subject <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> <info:fedora/$book_pid>
       or
       \$subject <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/$book_pid>
      )
    and
      (
      \$subject <info:fedora/fedora-system:def/model#hasModel> <info:fedora/islandora:pageCModel>
      )
    )
QUERY;
  $page_pids = array();
  $page_objects = $repository->ri->itqlQuery($query, 'unlimited');
  foreach ($page_objects as $page_object) {
    $page_pids[] = $page_object['subject']['value'];
  }
  return $page_pids;
}

function islandora_migrate_book($book_pid, $page_pids, $new_collection_pid, $repository_49, $repository_50) {
  // create new book object.
  $source_book_object = $repository_49->getObject($book_pid);
  $new_book_object = $repository_50->constructObject('islandora');
  $new_book_object->label = $source_book_object->label;
  foreach ($source_book_object as $dsid => $content) {
    if ($dsid != 'PDF') {
      $new_book_object->ingestDatastream($content);
    }
  }
  $new_book_object->relationships->changeObjectID($new_book_object->id);
  $new_book_object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', NULL);
  $new_book_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $new_collection_pid);
  $new_book_object->relationships->remove(FEDORA_MODEL_URI, 'hasModel', NULL);
  $new_book_object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:bookCModel');
  $ingested_object = islandora_add_object($new_book_object);
  $new_book_pid = $ingested_object->id;

  foreach ($page_pids as $page_pid) {
    $source_page_object = $repository_49->getObject($page_pid);
    $new_page_object = $repository_50->constructObject('islandora');
    $new_page_object->label = $source_page_object->label;
    $fakepop = count($source_page_object);
    if ($source_page_object['TIFF']) {
      $tiff = $source_page_object['TIFF']->content;
      add_renamed_datastream($new_page_object, 'OBJ', $tiff, 'image/tif');
    }
    else {
      add_renamed_datastream($new_page_object, 'JPG', $source_page_object['JPEG']->content, 'image/jpeg');
      add_renamed_datastream($new_page_object, 'TN', $source_page_object['TN']->content, 'image/jpeg');
    }

    add_renamed_datastream($new_page_object, 'OCR', $source_page_object['OCR']->content, 'text/plain');
    add_renamed_datastream($new_page_object, 'DC', $source_page_object['DC']->content, 'text/xml');
    add_renamed_datastream($new_page_object, 'RELS-EXT', $source_page_object['RELS-EXT']->content, 'application/rdf+xml');
    $new_page_object->relationships->changeObjectID($new_page_object->id);
    $new_page_object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOf', NULL);
    $new_page_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $new_book_pid);
    $new_page_object->relationships->remove(FEDORA_RELS_EXT_URI, 'isPageOf', NULL);
    $new_page_object->relationships->add(FEDORA_RELS_EXT_URI, 'isPageOf', $new_book_pid);
    $new_object = islandora_add_object($new_page_object);
    $new_pid = $new_object->id;
    $pause = NULL;
  }
}

function migrate_books_create_repository($fedoraUrl) {
  $username = "fedoraAdmin";
  $password = "anotherFed0r@@dmin";
  $connection = new RepositoryConnection($fedoraUrl, $username, $password);
  $connection->reuseConnection = TRUE;
  $repository = new FedoraRepository(new FedoraApi($connection), new SimpleCache());
  return $repository;
}

function migrate_books_build_new_collection($repository_49, $repository_50, $collection_pid) {
  $source_collection_object = $repository_49->getObject($collection_pid);
  $new_collection_object = $repository_50->constructObject('islandora');
  $new_collection_object->label = $source_collection_object->label;
  foreach ($source_collection_object as $dsid => $content) {
    $new_collection_object->ingestDatastream($content);
  }
  $new_collection_object->relationships->changeObjectID($new_collection_object->id);
  $new_collection_object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', NULL);
  $new_collection_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', 'islandora:9406');
  $ingested_object = islandora_add_object($new_collection_object);
  return $ingested_object->id;
}

function add_renamed_datastream($fedora_object, $dsid, $content, $mimetype) {
  $new_datastream = $fedora_object->constructDatastream($dsid);
  $new_datastream->label = $dsid;
  $new_datastream->mimetype = $mimetype;
  $new_datastream->setContentFromString($content);
  $fedora_object->ingestDatastream($new_datastream);
}