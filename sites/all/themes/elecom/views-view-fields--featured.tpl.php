<?php foreach ($fields as $id => $field): ?>
<?php
//dpm($id);
//dpm($field);
?>
<?php print $field->wrapper_prefix; ?>
<?php print $field->label_html; ?>
<?php print $field->content; ?>
<?php print $field->wrapper_suffix; ?>
<?php endforeach; ?>
