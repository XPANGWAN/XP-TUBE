# XP-TUBE Platform | منصة XP-TUBE

## English

The XP Platform module defines backend-neutral lifecycle, controller, display, audio-focus, text-input, and dialog event contracts, together with a bounded event queue and an XP-owned runtime boundary.

The application currently routes the existing SDL event stream through XP Runtime/EventQueue. SDL remains the single owner of initialization and polling; XP does not initialize SDL again or perform a second poll. During migration, XP events are temporarily translated back to the inherited screen-event vocabulary.

XP currently owns the PS4 OpenOrbis system-service bootstrap, system language ID lookup, timezone query, and PS4 numeric-language-to-BCP-47 mapping. Controller discovery and polling are still performed by the inherited SDL platform layer. The next migration slice introduces XP-owned controller identity/lifecycle primitives without changing SDL ownership or duplicating hardware access.

Display/presentation, audio focus, and system dialogs remain in the migration layer. This is a staged migration, not a complete independent PS4 backend.

Host tests validate the backend-neutral contracts and state logic. They do not prove PS4 hardware behavior; a real-console test remains required.

## العربية

تعرّف وحدة XP Platform عقوداً مستقلة عن نوع المنصة لدورة التشغيل، ووحدة التحكم، والشاشة، وتركيز الصوت، وإدخال النص، وحوارات النظام، إضافة إلى طابور أحداث محدود وواجهة وقت تشغيل مملوكة لـ XP.

يمرّر التطبيق حالياً تدفق أحداث SDL الحالي عبر XP Runtime/EventQueue. تظل SDL المالك الوحيد للتهيئة وقراءة الأحداث؛ ولا تقوم XP بتهيئة SDL مرة أخرى أو بقراءة الأحداث للمرة الثانية. وخلال الترحيل، تُحوّل أحداث XP مؤقتاً إلى صيغة الأحداث التي تستخدمها الشاشات الموروثة.

تمتلك XP حالياً تهيئة خدمات نظام PS4 عبر OpenOrbis، وقراءة معرّف لغة النظام، والاستعلام عن المنطقة الزمنية، وتحويل معرّفات لغات PS4 الرقمية إلى BCP-47. ما زالت طبقة SDL الموروثة مسؤولة عن اكتشاف وحدات التحكم وقراءة أحداثها. وتضيف الخطوة التالية أدوات XP لإدارة هوية وحدات التحكم ودورة اتصالها دون تغيير ملكية SDL أو تكرار الوصول إلى العتاد.

ما زالت الشاشة والعرض وتركيز الصوت وحوارات النظام ضمن طبقة الترحيل. هذا ترحيل تدريجي وليس تنفيذاً مستقلاً كاملاً لمنصة PS4.

تتحقق اختبارات المضيف من العقود المستقلة ومنطق الحالة، لكنها لا تثبت عمل العتاد على PS4؛ لذلك يظل الاختبار على جهاز حقيقي مطلوباً.
