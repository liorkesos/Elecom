<?php foreach ($fields as $id => $field): ?>
<?php
print $field->wrapper_prefix; 
print $field->label_html; 
/*
if ($id == 'field_image'){
  $file = file_load($field->raw);
  $img = file_create_url($file->uri);
  print '<img src="' . $img . '">';
}else{
print $field->content;
}
*/
print $field->content;
print $field->wrapper_suffix;
?>

<?php endforeach; ?>
