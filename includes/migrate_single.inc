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
/*
 * Migrates single book object from one repository to another
 * Normally books are moved collection by collection
 * This function is mostly for cleanup work
 */
function migrate_single_form(array $form, array &$form_state) {
  $form = array();
  $form['source'] = array(
    '#type' => 'textfield',
    '#description' => "Enter pid of d6 book to me moved.",
    '#default_value' => 'islandora:',
  );
  $form['destination'] = array(
    '#type' => 'textfield',
    '#description' => "Enter d7 destination collection ",
    '#default_value' => 'islandora:',
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Migrate this book.'),
  );
  return $form;
}

function migrate_single_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'migrate_books', 'includes/book_migrate');
  $source = $form_state['values']['source'];
  $destination = $form_state['values']['destination'];
  islandora_move_single_book($source, $destination);

}
/*
 * Migrates single book
 */
function islandora_move_single_book($source_book, $destination_collection){
  $fedora_49_url = "http://modernistcommons.ca:8080/fedora";
  $fedora_50_url = "http://137.149.200.50:8080/fedora";
  $repository_49 = migrate_books_create_repository($fedora_49_url);
  $repository_50 = migrate_books_create_repository($fedora_50_url);;
  $source_pages = islandora_migrate_get_page_pids($source_book, $repository_49);
  islandora_migrate_book($source_book, $source_pages, $destination_collection, $repository_49, $repository_50);
  return "Success";
}