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

function migrate_ocr_by_collection_form(array $form, array &$form_state) {
  $form = array();
  $form['source'] = array(
    '#type' => 'textfield',
    '#description' => "Enter pid of d6 critical edition to be mined.",
    '#default_value' => 'islandora:',
  );
  $form['destination'] = array(
    '#type' => 'textfield',
    '#description' => "Enter d7 destinatin book object ",
    '#default_value' => 'islandora:',
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Migrate these images.'),
  );
  return $form;
}

function migrate_ocr_by_collection_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'migrate_books', 'includes/book_migrate');
  $fedora_49_url = "http://modernistcommons.ca:8080/fedora";
  $fedora_50_url = "http://137.149.200.50:8080/fedora";
  $repository_49 = migrate_books_create_repository($fedora_49_url);
  $repository_50 = migrate_books_create_repository($fedora_50_url);
  $source = $form_state['values']['source'];
  $destination = $form_state['values']['destination'];

  $crited_page_pids = migrate_books_get_page_pids($repository_49, $source);
  $book_page_pids = migrate_books_get_page_pids($repository_50, $destination);
  $pairings = array();
  for ($count = 0; $count < count($crited_page_pids); $count++) {
    $pairings[] = array('source' => $crited_page_pids[$count], 'destination' => $book_page_pids[$count]);
  }

  $batch = array(
    'title' => t('Migrating OCR'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'migrate_books') . '/includes/migrate_ocr.inc',
    'progress_message' => t('@current of @total objects created.'),
  );

  foreach ($pairings as $pairing) {

    $batch['operations'][] = array('migrate_book_update_ocr', array(
        'repository_49' => $repository_49,
        'repository_50' => $repository_50,
        'pairing' => $pairing,
    ));
  }

  batch_set($batch);
  batch_process();
}

function migrate_books_get_page_pids($repository, $source) {
  $query = <<<XML
  select \$object \$page from <#ri>
    where \$object <fedora-rels-ext:isMemberOf> <info:fedora/$source>
    and (\$object <info:islandora/islandora-system:def/paged-item-info#sequenceNumber> \$page
    or \$object <http://islandora.ca/ontology/relsext#isSequenceNumber> \$page)
  order by \$page
XML;

  $pids = array();
  $objects = $repository->ri->itqlQuery($query, 'unlimited');
  foreach ($objects as $object) {
    $pids[] = $object['object']['value'];
  }
  return $pids;
}

function migrate_book_update_ocr($repository_49, $repository_50, $pairing) {
  $crited_page = $repository_49->getObject($pairing['source']);
  $destination_page = $repository_50->getObject($pairing['destination']);
  $content = new SimpleXMLElement($crited_page['CWRC']->content);
  $tei = $content->body;
  $cleaned_tei = strip_tags($tei->asXML());
  $destination_page['OCR']->content = $cleaned_tei;
}
