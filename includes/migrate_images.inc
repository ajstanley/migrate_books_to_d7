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

function migrate_images_by_collection_form(array $form, array &$form_state) {
  $form = array();
  $form['source'] = array(
    '#type' => 'textfield',
    '#description' => "Enter pid of d6 collection to be migrated.",
    '#default_value' => 'islandora:',
  );
  $form['destination'] = array(
    '#type' => 'textfield',
    '#description' => "Enter d7 destination collection",
    '#default_value' => 'islandora:',
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Migrate these images.'),
  );
  return $form;
}

function migrate_images_by_collection_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'migrate_books', 'includes/book_migrate');
  $fedora_49_url = "http://modernistcommons.ca:8080/fedora";
  $fedora_50_url = "http://137.149.200.50:8080/fedora";
  $repository_49 = migrate_books_create_repository($fedora_49_url);
  $repository_50 = migrate_books_create_repository($fedora_50_url);
  $source = $form_state['values']['source'];
  $destination = $form_state['values']['destination'];
  $image_pids = migrate_books_get_image_pids($repository_49, $source);
  foreach ($image_pids as $image_pid) {
    migrate_books_build_new_image($repository_49, $repository_50, $image_pid, $destination);
  }
}

function migrate_books_get_image_pids($repository_49, $source) {
  $query = <<<QUERY
select \$subject from <#ri> 
    where (
      (\$subject <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> <info:fedora/$source>
       or
       \$subject <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/$source>
      )
    and
      (
      \$subject <info:fedora/fedora-system:def/model#hasModel> <info:fedora/islandora:sp_large_image_cmodel>
      or
      \$subject <info:fedora/fedora-system:def/model#hasModel> <info:fedora/sp_basic_image_cmodel>
      )
    )
QUERY;

  $image_pids = array();
  $image_objects = $repository_49->ri->itqlQuery($query, 'unlimited');
  foreach ($image_objects as $image_object) {
    $image_pids[] = $image_object['subject']['value'];
  }
  return $image_pids;
}

function migrate_books_build_new_image($repository_49, $repository_50, $source_pid, $destination_dir_pid) {

  $source_image_object = $repository_49->getObject($source_pid);
  $new_image_object = $repository_50->constructObject('islandora');
  $new_image_object->label = $source_image_object->label;
  foreach ($source_image_object as $dsid => $content) {
    $new_image_object->ingestDatastream($content);
  }
  $new_image_object->relationships->changeObjectID($new_image_object->id);
  $new_image_object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', NULL);
  $new_image_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $destination_dir_pid);
  $ingested_object = islandora_add_object($new_image_object);
  return $ingested_object->id;
}