

```json
  {
     "title": "Hello!",
     "author" : {
       "givenName" : "John",
       "familyName" : "Doe",
       "age": 31,
       "family": {
         "mother": {
           "givenName": "Anna",
           "familyName" : "Doe",
           "age": 55
         }
       }
     },
     "tags":[ "example", "sample" ],
     "content": "This will be unchanged"
   }
```

```

Filters:

  /**/[familyName = 'Doe']

  /**/[[? = 'familyName'] = 'Doe']

  /**/familyName

  /**/[familyName like /^D.*/]

  /**/[familyName like 'D*']

  /author/**/mother/[age > 50]

  /author/**/mother/[age > 50 and age < 100]

  /**/[tags = ['example', 'sample']]

  /tags/[* = 'sample']

  /tags/[1 = 'sample']

  /tags/[* in ['sample', 'foo']]

  /tags/[* in ['sample', 'foo'] and [? like 'ta*']]

Projections:

  /**/[familyName = 'Doe'] | all

  /**/[familyName = 'Doe'] | /**/tags + /author/{givenName,familyName}

  /**/[familyName = 'Doe'] | all - /**/author/{givenName,familyName}

  /**/[familyName = 'Doe'] and /**/tags/[* = 'sample'] | /**/tags + /author/{givenName,familyName}

```