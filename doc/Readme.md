# Generating documentation
```
xmllint --noout --valid probackup.xml
xsltproc stylesheet.xsl probackup.xml >pg_backup.html
```
